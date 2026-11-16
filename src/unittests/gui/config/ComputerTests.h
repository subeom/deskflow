/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ComputerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  // Test are run in order top to bottom
  void initTestCase();
  void basicFunctionality();
  void crossOSSettings_defaultToDisabled();
  void crossOSF18Action_loadsMissingInvalidAndLegacyValuesAsPassThrough();
  void crossOSInvertScope_loadsMissingAndInvalidValuesAsAll();
  void crossOSDialog_saveSerializeAndProduceOptions();
  void shareClipboard_defaultsOnAndAddsNoOption();
  void shareClipboard_off_isSavedLoadedAndProducesTheOption();
  void shareClipboard_dialogChecksTheBoxAndSerializes();
  void monitorLayout_isEmptyAndAddsNothingByDefault();
  void monitorLayout_isSavedLoadedAndReadByTheServer_data();
  void monitorLayout_isSavedLoadedAndReadByTheServer();
  void monitorLayout_isLoadedFromAFileThatWasEditedByHand();
  void monitorLayout_takesPartInEquality();
  void monitorLayoutDialog_isOffUntilTheExperimentIsOn();
  void monitorLayoutDialog_followsTheServerDialog();
  void monitorLayoutDialog_saysWhetherTheLayoutIsValid();
  void monitorLayoutDialog_savesTheLayoutAsTheServerReadsIt();
  void monitorLayoutDialog_doesNotSaveALayoutThatIsWrong();
  void monitorLayoutDialog_anAcceptThatIsRefusedLeavesTheLayoutAlone();
  void monitorLayoutDialog_tabLeavesTheTextBox();
  void monitorLayoutDialog_importsWhatTheDiagnosticToolsPrint_data();
  void monitorLayoutDialog_importsWhatTheDiagnosticToolsPrint();
  void monitorLayoutDialog_importOfAFileThatIsNotAFit_saysSoAndChangesNothing();
  void arrangementDialog_aZoomThatTheSpinBoxCannotShow_isKeptWhenNothingIsTouched();
  void arrangementDialog_theServer_hasNoMonitorsToSayButAScale();

private:
  inline static const QString m_settingsPath = QStringLiteral("tmp/test");
  inline static const QString m_settingsFile = QStringLiteral("%1/Deskflow.conf").arg(m_settingsPath);
  inline static const QString m_stateFile = QStringLiteral("%1/Deskflow.state").arg(m_settingsPath);
};
