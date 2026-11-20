/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "AwdlModel.h"

#include <QString>

namespace deskflow::awdl {

/*!
 * What the window says of the option, in the language of the program: one place for the sentences of the card of the
 * settings, of the banner and of the dialog of the password, so that they say the same things (the context of the
 * translation is "AwdlTexts"). They are plain sentences of the program: nothing in them is the text of a person, and
 * nothing is put together from what a person typed.
 */

//! What the state of the link is, as a sentence (the numbers of the last burst are measurementText())
QString stateText(Summary summary);

//! "Last measurement: median 3.8 ms, worst 140 ms."; nothing when no burst was answered
QString measurementText(const BurstStats &stats);

//! The line of the banner: the link is on and the latency has the spikes (with the numbers when there are any)
QString bannerText(const BurstStats &stats);

//! The sentence that the dialog of the password of the system shows (osascript puts its own words around it)
QString prompt(Direction direction);

//! Milliseconds as a person reads them: one decimal under 10 ms, none above
QString formatMs(double milliseconds);

} // namespace deskflow::awdl
