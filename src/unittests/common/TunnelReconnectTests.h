/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelReconnectTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void theFirstRetryIsQuick();
  void theNextRetriesKeepTheSecondOfADirectLink_data();
  void theNextRetriesKeepTheSecondOfADirectLink();
  void aLongOutageEasesTheRetriesAndNeverStopsThem();
  void everyIntervalIsUsable();
  void aPolicyThatIsGivenIsFollowed();
};
