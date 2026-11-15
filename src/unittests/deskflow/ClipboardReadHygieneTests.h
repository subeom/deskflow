/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ClipboardReadHygieneTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void pngSize_readsTheHeader();
  void pngSize_rejectsWhatIsNotAPng_data();
  void pngSize_rejectsWhatIsNotAPng();
  void tiffSize_readsTheFirstDirectoryInEitherByteOrder_data();
  void tiffSize_readsTheFirstDirectoryInEitherByteOrder();
  void tiffSize_rejectsWhatIsNotATiff_data();
  void tiffSize_rejectsWhatIsNotATiff();
  void bitmapLowerBound_isFourBytesAPixelAndNeverWraps();
  void surelyOverLimit_countsWhatEveryClipboardMessageAdds_data();
  void surelyOverLimit_countsWhatEveryClipboardMessageAdds();
  void cache_returnsWhatWasStoredUntilItIsInvalidated();
  void cache_keepsFormatsApart();
  void cache_doesNotKeepWhatIsTooLarge();
  void cache_staysWithinItsBudget();
};
