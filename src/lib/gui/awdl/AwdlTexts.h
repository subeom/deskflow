/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "AwdlModel.h"
#include "AwdlPermission.h"

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

//! What the permission to change the link with no password is, as a sentence: Allowed, Not allowed (with what the
//! program does then), or Unknown. \p filePresent adds that the file of the program is there when sudo does not use it
QString permissionText(PermissionState state, bool filePresent);

//! What came of the last thing that was asked for: a sentence, or nothing for None
QString permissionNoteText(PermissionNote note);

//! The question that is asked before the dialog of the password opens for the permission, in plain words: the title,
//! and what is added, what it lets the account do, who else can do it, and how to remove it
QString confirmAllowTitle();
QString confirmAllowText();

//! The sentence that the dialog of the password of the system shows for adding and for removing the file
QString permissionPrompt(PermissionAction action);

//! Milliseconds as a person reads them: one decimal under 10 ms, none above
QString formatMs(double milliseconds);

} // namespace deskflow::awdl
