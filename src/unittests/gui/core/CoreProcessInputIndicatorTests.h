/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class CoreProcessInputIndicatorTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void coreClientEncodesSubscription();
  void validLocalAndActivityAreForwarded();
  void malformedStaleAndDuplicateActivityInvalidate();
  void resetDropsLocalState();
  void rawClientBoundaryRejectsMalformedIndicatorMessages();
  void rawClientBoundaryForwardsOnlyExactIndicatorMessages();
};
