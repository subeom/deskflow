/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class AbsolutePointerTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void normalised_data();
  void normalised();
  void normalised_negativeIsTruncatedTowardZero();
  void normalised_degenerateExtent_isZero();
};
