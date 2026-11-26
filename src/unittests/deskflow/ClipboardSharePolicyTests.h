/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QTest>

class ClipboardSharePolicyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void limit_isTenKiBOfMarshalledClipboard();
  void off_sharesEverythingAsIs_data();
  void off_sharesEverythingAsIs();
  void on_decide_data();
  void on_decide();
  void on_decide_countsBytesOfUtf8_data();
  void on_decide_countsBytesOfUtf8();
  void on_apply_keepsOnlyTheText();
  void on_apply_emptiesWhatIsNotShared_data();
  void on_apply_emptiesWhatIsNotShared();
  void on_apply_keepsTextThatFits();
  void sizeOption_data();
  void sizeOption();
  void rejectionLog_reportsEachRejectionOnce();
  void clipboard_textOnly_ignoresOtherFormats();
  void clipboard_textOnly_keepsWhatItHolds();
  void clipboard_textOnly_unmarshallsTextOnly();
  void copy_intoTextOnlyClipboard_neverReadsOtherFormats();
  void copy_intoDefaultClipboard_readsEveryFormat();

private:
  Log m_log;
};
