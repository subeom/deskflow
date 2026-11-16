/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

class ArrangementConfigTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  // The server configuration that the classic grid makes. Recorded from the code before the arrangement was added;
  // with the experiment off, or no arrangement used, it has to stay byte for byte what it is.
  void classicGrid_generatesTheConfigItAlwaysDid_data();
  void classicGrid_generatesTheConfigItAlwaysDid();

  // The server configuration that the arrangement makes (experimental)
  void arrangement_isUsedOnlyWhenTheExperimentAndTheUseAreOn_data();
  void arrangement_isUsedOnlyWhenTheExperimentAndTheUseAreOn();
  void arrangement_ofEqualComputersInTheClassicGrid_isTheClassicText_data();
  void arrangement_ofEqualComputersInTheClassicGrid_isTheClassicText();
  void arrangement_theServerOfTheUser_makesPartialLinks();
  void arrangement_aComputerThatIsNotPlaced_isBesideTheOthers();
  void arrangement_isSavedAndLoaded();
  void arrangement_oldConfigurationsLoadAsTheyWere();
  void arrangement_classicGridConversion_keepsTheLinks();
  void reportedScreen_isParsedFromTheLogOfTheCore_data();
  void reportedScreen_isParsedFromTheLogOfTheCore();
  void reportedScreen_isTakenFromTheLogOnlyWhileTheExperimentIsOnAndOnlyForTheComputersOfTheConfig();
  void reportedMonitors_isParsedFromTheLogOfTheCore_data();
  void reportedMonitors_isParsedFromTheLogOfTheCore();
  void reportedMonitors_textRoundTripsAndOnlyValidTextIsTaken_data();
  void reportedMonitors_textRoundTripsAndOnlyValidTextIsTaken();
  void reportedMonitors_areTakenOnlyWhileTheExperimentIsOnAndOnlyForTheComputersOfTheConfig();
  void reportedMonitors_areRememberedForAComputerThatIsOffline();
  void reportedMonitors_areDrawnForAComputerThatHasNoLayout();
  void reportedMonitors_aTypedLayoutIsWhatIsDrawn();
  void reportedMonitors_aListenerIsToldOfEveryReport();
  void assumedSizes_areOfTheComputersThatHaveNoMonitorsAndNoSize();
  void monitorEditing_drawsWhatIsStored_andStoresWhatIsMade();
  void setComputerSize_isTheSizeOfAComputerThatHasNone_andTheMonitorOfOneThatHasOne();
  void setComputerSize_neverStoresWhatALayoutCannotHave();
  void setComputerZoom_scalesTheWholeComputer_andKeepsItsMonitorsAndItsSize();
  void reportedScreen_doesNotChangeWhatIsGenerated();
  void reportedScreen_isAdoptedAsTheSizeOfAComputerThatHasNone();

private:
  QTemporaryDir m_settingsDir;
};
