/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class OSXInputSourceTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void exactIdsAreClassified();
  void otherValuesAreUnknown();
  void transitionActivationRequiresAChangedKnownSource_data();
  void transitionActivationRequiresAChangedKnownSource();
};
