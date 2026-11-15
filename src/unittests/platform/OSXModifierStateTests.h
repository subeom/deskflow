/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class OSXModifierStateTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void leftCommandTracksDownAndUp();
  void rightCommandTracksDownAndUp();
  void bothCommandsLeftReleasedFirst();
  void bothCommandsRightReleasedFirst();
  void duplicateStateDoesNotEmitTransition();
  void legacyCommandButtonNormalizesRightSide();
  void setDownSeedsExistingState();
};
