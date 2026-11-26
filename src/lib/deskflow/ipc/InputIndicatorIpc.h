/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>

namespace deskflow::input {

enum class InputIndicatorSource
{
  LatinABC,
  Korean2Set,
  Unknown
};

enum class InputIndicatorActivityKind
{
  LocalPrimaryMouseClick,
  LocalPrimaryKeyDown
};

struct InputIndicatorActivity
{
  uint64_t sequence = 0;
  uint64_t eventTimestampMach = 0;
  InputIndicatorActivityKind kind = InputIndicatorActivityKind::LocalPrimaryKeyDown;
};

inline constexpr auto kInputIndicatorActivityCommand = "inputIndicatorActivity";
inline constexpr auto kInputIndicatorLocalCommand = "inputIndicatorLocal";
inline constexpr auto kInputIndicatorSubscriptionCommand = "inputIndicatorSubscription";
inline constexpr auto kInputIndicatorCapabilityEnvironment = "DESKFLOW_INPUT_INDICATOR_CAPABILITY";
inline constexpr auto kInputIndicatorGuiPidEnvironment = "DESKFLOW_INPUT_INDICATOR_GUI_PID";
inline constexpr int kInputIndicatorCapabilityLength = 64;
inline constexpr int kMaximumInputIndicatorRawMessageSize = 256;
inline constexpr int kInputIndicatorMaximumEventAgeMilliseconds = 1000;

enum class InputIndicatorRawMessageState
{
  NotIndicator,
  Invalid,
  Valid
};

struct InputIndicatorRawMessage
{
  InputIndicatorRawMessageState state = InputIndicatorRawMessageState::NotIndicator;
  QString command;
  QString payload;
};

QString encodeInputIndicatorActivity(const InputIndicatorActivity &activity);
std::optional<InputIndicatorActivity> decodeInputIndicatorActivity(const QString &payload);
QString encodeInputIndicatorBoolean(bool value);
std::optional<bool> decodeInputIndicatorBoolean(const QString &payload);
QString encodeInputIndicatorSubscription(bool enabled, const QString &capability);
std::optional<bool> decodeInputIndicatorSubscription(const QString &payload, const QString &expectedCapability);
bool isValidInputIndicatorCapability(const QString &capability);
InputIndicatorRawMessage parseInputIndicatorRawMessage(const QByteArray &raw);
QString redactInputIndicatorMessage(const QByteArray &raw);
bool isInputIndicatorActivityFresh(
    const InputIndicatorActivity &activity, uint64_t nowTimestampMach, uint64_t maximumAgeMach
);
bool isInputIndicatorActivityNewer(const InputIndicatorActivity &activity, uint64_t previousSequence);

void setInputIndicatorSubscribed(bool subscribed);
bool isInputIndicatorSubscribed();
uint64_t inputIndicatorSubscriptionEpoch();
void setInputIndicatorLocal(bool local);
bool isInputIndicatorLocal();

} // namespace deskflow::input
