/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/widgets/TunnelSettingsGroup.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QComboBox;
class QWidget;

namespace deskflow::gui::settings {

class SettingsBinder;
class SettingsPage;

/*!
 * \brief The pages of the settings of the main window of the new look, with the controls bound to the settings.
 *
 * The sidebar lists them in the order of entries(), the window puts them in its stack (MainShell::addSettingsPage). A
 * change is written at once (SettingsBinder) and reported by applied(), so that the window can show what it takes (see
 * SettingEffects.h). The pages are made from the settings that the dialogs of the system look have, and the texts that
 * these have are taken from them (the context of the dialog), so that a language that is translated for them is
 * translated here.
 *
 * Where a later page puts a card: SettingsPage::addCard() on the page of entries(); a new page is a new entry in
 * pageIds() and a builder that adds it. The page of the client configuration (with the group of the tunnel) is meant
 * to be one of them.
 */
class SettingsPages : public QObject
{
  Q_OBJECT

public:
  struct Entry
  {
    QString id;
    //! A name of the icons of the style layer
    QString icon;
    std::function<QString()> title;
    SettingsPage *page = nullptr;
    //! A page that the sidebar has no entry for (the entry of this page is marked while it is shown); empty: it has one
    QString parentId;
  };

  /**
   * \p tunnel is what the group of the Cloudflare Tunnel asks of its surroundings; by default the secure store of the
   * system and the real file system. Tests give it a store in memory. The group is made when the page of the client is
   * first shown (not when the program starts): the store of the system can ask the user for a password, which is for
   * the person who looks at the page to meet and not the one who starts the program.
   */
  explicit SettingsPages(QObject *parent = nullptr, TunnelSettingsDependencies tunnel = {});

  //! The pages in the order of the sidebar
  QList<Entry> entries() const;
  SettingsPage *page(const QString &id) const;
  SettingsBinder *binder() const;
  //! The group of the Cloudflare Tunnel of the page of the client; null until the page is first shown
  TunnelSettingsGroup *tunnelGroup() const;

  //! The id of the page that "Preferences" opens
  static QString generalId();
  //! The id of the page of the client (the settings that the dialog "Client Configuration" has)
  static QString clientId();

Q_SIGNALS:
  //! A control changed a setting (and it was written)
  void applied(const QString &key);
  //! The person asked to see the fingerprint of this computer
  void fingerprintRequested();
  //! The person confirmed that all the settings are to be removed
  void removeAllSettingsRequested();
  //! The settings of the Cloudflare Tunnel were saved with the button of the page (the window reads them again)
  void tunnelSaved();
  //! The list of the servers that were typed in the address field was cleared (the field has the list again)
  void serverHistoryCleared();
  //! The person asked to go to another page of the settings (the id of the page)
  void pageRequested(const QString &id);

private:
  SettingsPage *buildGeneral();
  SettingsPage *buildSecurity();
  SettingsPage *buildDisplay();
  SettingsPage *buildKeyboard();
  SettingsPage *buildNetwork();
  SettingsPage *buildClient();
  SettingsPage *buildAdvanced();
  void fillAddresses(QComboBox *combo);

  SettingsBinder *m_binder = nullptr;
  TunnelSettingsDependencies m_tunnel;
  QPointer<TunnelSettingsGroup> m_tunnelGroup;
  QList<Entry> m_entries;
};

} // namespace deskflow::gui::settings
