/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

class AppearanceTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void mode_isReadFromTextAndWrittenBack();
  void configuredMode_isTheNewLookUntilAskedOtherwise();
  void configuredMode_isTheDefaultOfTheSetting();
  void configuredMode_theSettingAsks_andTheEnvironmentOverrides();
  void configuredMode_followsTheTableOfTheSettingAndTheEnvironment_data();
  void configuredMode_followsTheTableOfTheSettingAndTheEnvironment();

  void firstRun_withoutAFileOpensTheNewLook_andWritesNothing();
  void anUpgrade_withTheSystemLookStoredKeepsItAtEveryStart();
  void anUpgrade_withAFileThatHasNoLook_opensTheNewLook();

  void apply_system_leavesTheApplicationAsToday();
  void apply_systemOnKde_leavesTheStyleToKde();
  void apply_deskflow_installsTheStyleAndItsPalette();
  void apply_withoutAChoice_installsTheNewLook();

  void currentTokens_withoutTheStyle_areFlatAndOpaqueInTheColoursOfTheOs();
  void currentTokens_withTheStyle_areTheOnesOfItsLook();

private:
  QTemporaryDir m_settingsDir;
};
