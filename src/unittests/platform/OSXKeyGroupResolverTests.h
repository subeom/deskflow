/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class OSXKeyGroupResolverTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void enabledLayoutResolvesExactlyWithoutAsciiLookup();
  void inputMethodLayoutResolvesThroughAsciiLayout();
  void unresolvedFallbackSkipsGroupsWithoutKeyLayout();
  void unresolvedOutcomeIsReportedOncePerDistinctInput();
  void setGroupsResetsReportedOutcomes();
  void unreadableLayoutKeepsPreviousGroup();
  void unreadableLayoutWithoutHistoryFallsBack();
  void noGroupsResolvesToFirstGroup();
};
