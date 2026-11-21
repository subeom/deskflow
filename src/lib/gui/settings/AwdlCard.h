/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QPointer>

class QPushButton;
class SettingCard;
class SettingRow;

namespace deskflow::awdl {
class AwdlController;
}

namespace deskflow::gui::settings {

class SettingsBinder;
class SettingsPage;

/*!
 * \brief The card of the Network page of a Mac that has the option "turn off the wireless link of AirDrop and Handoff
 * while sharing": the switch (the setting), the line that says the state of the link with the numbers of the last
 * measurement, and the buttons that measure now, turn the link off now and turn it back on.
 *
 * It knows the controller and nothing of the system: the buttons ask the controller, and the line says what the
 * controller says. Without a controller (nothing to ask) the buttons are off and the line says that nothing was read.
 * It is made only where there is a wireless link of this kind (SettingsPages checks awdl::platformSupported()).
 */
class AwdlCard : public QObject
{
  Q_OBJECT

public:
  AwdlCard(SettingsPage *page, SettingsBinder *binder);

  SettingCard *card() const;
  //! The controller that the card shows and asks (not owned; null: none)
  void setController(deskflow::awdl::AwdlController *controller);

private:
  void update();
  QString stateLine() const;

  QPointer<SettingsPage> m_page;
  SettingCard *m_card = nullptr;
  SettingRow *m_stateRow = nullptr;
  QPushButton *m_measure = nullptr;
  QPushButton *m_turnOff = nullptr;
  QPushButton *m_turnOn = nullptr;
  QPointer<deskflow::awdl::AwdlController> m_controller;
};

} // namespace deskflow::gui::settings
