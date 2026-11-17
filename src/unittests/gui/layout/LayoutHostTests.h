/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

class LayoutHostTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  // the arrangement of the settings, as a layout
  void capture_isWhereTheComputersAre();
  void capture_isNothingWithoutAnArrangement();
  void apply_movesTheComputers_andWritesOnlyTheirPlaces();
  void apply_ofTheSameLayout_changesNothing();
  void apply_isAllOrNothing_whenItWouldOverlap();
  void apply_keepsTheComputersThatTheLayoutDoesNotKnow();
  void apply_putsAComputerThatWouldOverlapBesideTheOthers();
  void apply_ignoresTheCaseOfNames_andComputersThatAreGone();
  void apply_theZoomGoesThroughTheCanvasRule();
  void reattach_putsTheComputersAgainstTheServerOfTheNewDisplays();
  void generatedConfigText_isWhatTheCoreIsGiven();

  // the host
  void host_isEnabledOnlyWhenEverythingAsksForIt_data();
  void host_isEnabledOnlyWhenEverythingAsksForIt();
  void host_coreConfigDiffers_comparesWithTheTextTheCoreWasStartedWith();
  void host_applyWritesTheSettings_andAReloadSeesThem();
  void host_dialogOpen_isAModalWidgetByDefault();

  // the whole, with the settings and the config of the GUI
  void service_off_changesNothingAtAll();
  void service_followsTheDisplays_andComesBackToTheSameConfig();
  void service_undo_putsTheArrangementBack();
  void service_waitsForADialog();
  void service_configSaved_updatesTheProfile();
  void service_switchedOff_stops();
  void service_noticeText_saysWhatHappened();
  void service_instance_isTheLiveOne();
  void service_theStoreIsNextToTheSettings();
  void service_aNewerStore_isNotUsed();
  void service_switchedOff_forgetsTheSituation_andOnAgainAdoptsTheArrangement();
  void reattach_isSkippedWhenTheConfigSeesOtherDisplaysThanTheSignature();

  void displaysFromScreens_areOfTheScreens();

private:
  QTemporaryDir m_dir;
};
