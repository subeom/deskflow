/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

// The About dialog as cards (the new look): the credits are in cards, and the System look is the form as it was
class AboutCardsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void systemLook_isTheFormAsItWas();
  void deskflowLook_hasTheCreditsInCards_withTheHeaderAndTheButtonKept();
  void deskflowLook_isAsSmallAsTheFormWas_andTheCardsAreNotCutAtTheSide();
};
