/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QObject>
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
  };

  explicit SettingsPages(QObject *parent = nullptr);

  //! The pages in the order of the sidebar
  QList<Entry> entries() const;
  SettingsPage *page(const QString &id) const;
  SettingsBinder *binder() const;

  //! The id of the page that "Preferences" opens
  static QString generalId();

Q_SIGNALS:
  //! A control changed a setting (and it was written)
  void applied(const QString &key);
  //! The person asked to see the fingerprint of this computer
  void fingerprintRequested();
  //! The person confirmed that all the settings are to be removed
  void removeAllSettingsRequested();

private:
  SettingsPage *buildGeneral();
  SettingsPage *buildSecurity();
  SettingsPage *buildDisplay();
  SettingsPage *buildKeyboard();
  SettingsPage *buildNetwork();
  SettingsPage *buildAdvanced();
  void fillAddresses(QComboBox *combo);

  SettingsBinder *m_binder = nullptr;
  QList<Entry> m_entries;
};

} // namespace deskflow::gui::settings
