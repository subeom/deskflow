/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QFrame>
#include <net/Fingerprint.h>

class QLabel;

class FingerprintPreview : public QFrame
{
  Q_OBJECT
public:
  explicit FingerprintPreview(
      QWidget *parent, const Fingerprint &fingerprint = {}, const QString &titleText = {}, bool hashMode = false
  );
  ~FingerprintPreview() override = default;
  void toggleMode(bool hashMode);

  //! The hash in groups of \p groupSize characters, \p groupsPerLine to a line: "ABCD 1234 5678 9ABC" (the way that a
  //! person reads two of them side by side, not the bytes with colons)
  static QString groupedHash(const QByteArray &data, int groupSize = 4, int groupsPerLine = 4);
  //! Whether the hash is the first view and the image the second one (the new look); the image first is the form of
  //! the System look
  static bool hashFirst();

private:
  QLayout *emptyLayout();
  QLayout *sha256Layout(const Fingerprint &fingerprint = {}, const QString &titleText = {}, bool hashMode = false);
  QLabel *m_lblHash = nullptr;
  QLabel *m_lblArt = nullptr;
};
