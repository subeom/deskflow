/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/core/AddressClassifier.h"

#include <QDialog>
#include <QList>

class QDialogButtonBox;
class QTextBrowser;

/**
 * Lists every IPv4 address a client could connect to on this computer, on which network interface it is and what it is
 * for (LAN, VPN or overlay, virtual, link-local), so that a client is not given the address of the wrong network card.
 */
class ServerAddressesDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ServerAddressesDialog(QWidget *parent, const QList<deskflow::gui::AddressInfo> &addresses);

  //! Room for a table of a few addresses and the explanations: lines and letters of the font in use
  QSize sizeHint() const override;

protected:
  void changeEvent(QEvent *event) override;

private:
  void retranslate();

  QList<deskflow::gui::AddressInfo> m_addresses;
  QTextBrowser *m_view;
  QDialogButtonBox *m_buttons;
};
