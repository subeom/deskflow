/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

// The dialog of the fingerprints: the image first, as it was, in the System look; the hash in groups of four
// characters first in the new look
class FingerprintDialogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void groupedHash_isFourCharactersToAGroupAndFourGroupsToALine();
  void systemLook_showsTheImageFirst_andTheHashAsItWas();
  void deskflowLook_showsTheGroupedHashFirst_andTheImageOnRequest();
  void deskflowLook_comparesTwoGroupedHashes_withoutChangingSize();
};
