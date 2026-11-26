/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TrayState.h"

#include "gui/widgets/StatusChip.h"

#include <QWidget>

class QLabel;

namespace deskflow::gui::tray {

/*!
 * \brief The header of the tray menu where Qt draws the menu (Windows): the icon of the state, the state in a few words
 * with the sentence behind it, and a chip that says the state in one word. It is not to be chosen and does not take the
 * keyboard (a menu moves over it); a screen reader is told its three parts as one text.
 */
class TrayHeaderWidget : public QWidget
{
  Q_OBJECT

public:
  explicit TrayHeaderWidget(QWidget *parent = nullptr);

  void setContent(
      TrayState state, const QString &headline, const QString &detail, const QString &chipText,
      StatusChip::Kind chipKind
  );

  QString headline() const;
  QString detail() const;
  const StatusChip *chip() const;

  //! The name of an icon of the Phosphor set for a state
  static QString iconName(TrayState state);

private:
  class StateIcon;
  void updateIcon();
  void updateColors();

  TrayState m_state = TrayState::Idle;
  StateIcon *m_icon;
  QLabel *m_headline;
  QLabel *m_detail;
  StatusChip *m_chip;
};

} // namespace deskflow::gui::tray
