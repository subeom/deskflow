/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorIpc.h"

#include <QStringList>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace deskflow::input {

namespace {

std::atomic_bool s_subscribed{false};
std::atomic_uint64_t s_subscriptionEpoch{0};
std::atomic_bool s_local{false};

std::optional<uint64_t> parseUnsigned(const QString &value)
{
  if (value.isEmpty()) {
    return std::nullopt;
  }
  for (const auto character : value) {
    if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
      return std::nullopt;
    }
  }

  bool ok = false;
  const auto number = value.toULongLong(&ok);
  return ok ? std::optional(number) : std::nullopt;
}

} // namespace

namespace {

bool secureStringEquals(const QString &left, const QString &right)
{
  const auto leftBytes = left.toLatin1();
  const auto rightBytes = right.toLatin1();
  volatile unsigned difference = static_cast<unsigned>(leftBytes.size() ^ rightBytes.size());
  const auto length = std::max(leftBytes.size(), rightBytes.size());
  for (qsizetype index = 0; index < length; ++index) {
    const unsigned leftByte = index < leftBytes.size() ? static_cast<unsigned char>(leftBytes.at(index)) : 0;
    const unsigned rightByte = index < rightBytes.size() ? static_cast<unsigned char>(rightBytes.at(index)) : 0;
    difference |= leftByte ^ rightByte;
  }
  return difference == 0;
}

} // namespace

QString encodeInputIndicatorActivity(const InputIndicatorActivity &activity)
{
  const auto kind = activity.kind == InputIndicatorActivityKind::LocalPrimaryMouseClick ? 0 : 1;
  return QStringLiteral("1:%1:%2:%3")
      .arg(activity.sequence)
      .arg(activity.eventTimestampMach)
      .arg(kind);
}

std::optional<InputIndicatorActivity> decodeInputIndicatorActivity(const QString &payload)
{
  if (payload.contains(QLatin1Char('=')) || payload.contains(QLatin1Char('\n')) ||
      payload.contains(QLatin1Char('\r'))) {
    return std::nullopt;
  }

  const auto parts = payload.split(QLatin1Char(':'), Qt::KeepEmptyParts);
  if (parts.size() != 4 || parts.at(0) != QLatin1String("1")) {
    return std::nullopt;
  }

  const auto sequence = parseUnsigned(parts.at(1));
  const auto timestamp = parseUnsigned(parts.at(2));
  const auto kind = parseUnsigned(parts.at(3));
  if (!sequence || *sequence == 0 || !timestamp || *timestamp == 0 || !kind || *kind > 1) {
    return std::nullopt;
  }

  return InputIndicatorActivity{
      .sequence = *sequence,
      .eventTimestampMach = *timestamp,
      .kind = *kind == 0 ? InputIndicatorActivityKind::LocalPrimaryMouseClick
                         : InputIndicatorActivityKind::LocalPrimaryKeyDown,
  };
}

QString encodeInputIndicatorBoolean(bool value)
{
  return value ? QStringLiteral("1") : QStringLiteral("0");
}

std::optional<bool> decodeInputIndicatorBoolean(const QString &payload)
{
  if (payload == QLatin1String("1")) {
    return true;
  }
  if (payload == QLatin1String("0")) {
    return false;
  }
  return std::nullopt;
}

QString encodeInputIndicatorSubscription(bool enabled, const QString &capability)
{
  return QStringLiteral("%1:%2").arg(enabled ? 1 : 0).arg(capability);
}

std::optional<bool> decodeInputIndicatorSubscription(const QString &payload, const QString &expectedCapability)
{
  const auto parts = payload.split(QLatin1Char(':'), Qt::KeepEmptyParts);
  if (parts.size() != 2 || parts.at(1).isEmpty() || !isValidInputIndicatorCapability(parts.at(1)) ||
      !secureStringEquals(parts.at(1), expectedCapability)) {
    return std::nullopt;
  }
  return decodeInputIndicatorBoolean(parts.at(0));
}

bool isValidInputIndicatorCapability(const QString &capability)
{
  if (capability.size() != kInputIndicatorCapabilityLength) {
    return false;
  }
  for (const auto character : capability) {
    const bool lowerHex = character >= QLatin1Char('0') && character <= QLatin1Char('9');
    const bool upperHex = character >= QLatin1Char('a') && character <= QLatin1Char('f');
    if (!lowerHex && !upperHex) {
      return false;
    }
  }
  return true;
}

InputIndicatorRawMessage parseInputIndicatorRawMessage(const QByteArray &raw)
{
  constexpr std::array commands{
      kInputIndicatorSubscriptionCommand,
      kInputIndicatorActivityCommand,
      kInputIndicatorLocalCommand,
  };

  const char *matchedCommand = nullptr;
  qsizetype matchedAt = -1;
  for (const auto command : commands) {
    const auto prefix = QByteArray::fromRawData(command, static_cast<int>(strlen(command)));
    const auto found = raw.indexOf(prefix);
    if (found >= 0) {
      matchedCommand = command;
      matchedAt = found;
      break;
    }
  }
  if (!matchedCommand) {
    return {};
  }
  if (matchedAt != 0 || raw.size() > kMaximumInputIndicatorRawMessageSize) {
    return {.state = InputIndicatorRawMessageState::Invalid,
            .command = QString::fromLatin1(matchedCommand)};
  }

  for (const auto byte : raw) {
    if (byte <= 0x20 || byte > 0x7e) {
      return {.state = InputIndicatorRawMessageState::Invalid,
              .command = QString::fromLatin1(matchedCommand)};
    }
  }
  const auto separator = raw.indexOf('=');
  if (separator <= 0 || separator != raw.lastIndexOf('=') || separator + 1 >= raw.size()) {
    return {.state = InputIndicatorRawMessageState::Invalid,
            .command = QString::fromLatin1(matchedCommand)};
  }
  const auto command = raw.left(separator);
  if (command != QByteArray(matchedCommand)) {
    return {.state = InputIndicatorRawMessageState::Invalid,
            .command = QString::fromLatin1(matchedCommand)};
  }
  return {
      .state = InputIndicatorRawMessageState::Valid,
      .command = QString::fromLatin1(command),
      .payload = QString::fromLatin1(raw.mid(separator + 1)),
  };
}

QString redactInputIndicatorMessage(const QByteArray &raw)
{
  const auto parsed = parseInputIndicatorRawMessage(raw);
  if (parsed.state == InputIndicatorRawMessageState::NotIndicator) {
    return QString::fromUtf8(raw);
  }
  return QStringLiteral("%1=<redacted>").arg(parsed.command);
}

bool isInputIndicatorActivityFresh(
    const InputIndicatorActivity &activity, uint64_t nowTimestampMach, uint64_t maximumAgeMach
)
{
  if (activity.eventTimestampMach == 0 || nowTimestampMach < activity.eventTimestampMach) {
    return false;
  }
  return nowTimestampMach - activity.eventTimestampMach <= maximumAgeMach;
}

bool isInputIndicatorActivityNewer(const InputIndicatorActivity &activity, uint64_t previousSequence)
{
  return activity.sequence > previousSequence;
}

void setInputIndicatorSubscribed(bool subscribed)
{
  const auto previous = s_subscribed.exchange(subscribed, std::memory_order_acq_rel);
  if (previous != subscribed) {
    s_subscriptionEpoch.fetch_add(1, std::memory_order_release);
  }
}

bool isInputIndicatorSubscribed()
{
  return s_subscribed.load(std::memory_order_acquire);
}

uint64_t inputIndicatorSubscriptionEpoch()
{
  return s_subscriptionEpoch.load(std::memory_order_acquire);
}

void setInputIndicatorLocal(bool local)
{
  s_local.store(local, std::memory_order_release);
}

bool isInputIndicatorLocal()
{
  return s_local.load(std::memory_order_acquire);
}

} // namespace deskflow::input
