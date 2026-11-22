/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

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

QString encodeInputIndicatorActivity(const InputIndicatorActivity &activity);
std::optional<InputIndicatorActivity> decodeInputIndicatorActivity(const QString &payload);
QString encodeInputIndicatorBoolean(bool value);
std::optional<bool> decodeInputIndicatorBoolean(const QString &payload);
bool isInputIndicatorActivityFresh(
    const InputIndicatorActivity &activity, uint64_t nowTimestampMach, uint64_t maximumAgeMach
);
bool isInputIndicatorActivityNewer(const InputIndicatorActivity &activity, uint64_t previousSequence);

void setInputIndicatorSubscribed(bool subscribed);
bool isInputIndicatorSubscribed();
void setInputIndicatorLocal(bool local);
bool isInputIndicatorLocal();

} // namespace deskflow::input
