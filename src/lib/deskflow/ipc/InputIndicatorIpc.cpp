/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorIpc.h"

#include <QStringList>

#include <atomic>

namespace deskflow::input {

namespace {

std::atomic_bool s_subscribed{false};
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
  s_subscribed.store(subscribed, std::memory_order_release);
}

bool isInputIndicatorSubscribed()
{
  return s_subscribed.load(std::memory_order_acquire);
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
