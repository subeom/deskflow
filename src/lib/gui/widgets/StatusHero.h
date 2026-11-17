/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "StatusChip.h"
#include "gui/style/PanelWidget.h"

class QGridLayout;
class QHBoxLayout;
class QLabel;
class StatusOrb;

/*!
 * \brief The state of the program first: a round badge with an icon in the colour of the state, a headline ("Connected
 * to 2 computers"), a line of detail and the chips of the facts that go with it. A screen reader finds it as a group
 * that is named by the headline, and it is told when the headline changes (Qt 6.8 and later), so a state that comes
 * after a start or a stop is not seen only.
 */
class StatusHero : public deskflow::gui::style::PanelWidget
{
  Q_OBJECT

public:
  explicit StatusHero(QWidget *parent = nullptr);

  StatusChip::Kind kind() const;
  void setKind(StatusChip::Kind kind);

  //! The icon of the badge when it is not the one of the kind (a name of the Phosphor set)
  void setIconName(const QString &name);
  QString iconName() const;

  QString headline() const;
  void setHeadline(const QString &headline);

  QString detail() const;
  void setDetail(const QString &detail);

  StatusChip *addChip(const QString &text, StatusChip::Kind kind = StatusChip::Kind::Neutral);
  void clearChips();
  QList<StatusChip *> chips() const;

  //! A widget of the window under the chips (the address of the server); the hero takes it over
  void addAccessory(QWidget *widget);
  //! The buttons at the end of the hero (Start, Restart); they go under the text when the hero is narrower than
  //! kWrapWidth. The hero takes them over.
  void setActions(const QList<QWidget *> &actions);
  bool actionsAreBelow() const;

  static constexpr int kWrapWidth = 600;

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void applyLook();
  void placeActions();

  StatusOrb *m_orb = nullptr;
  QLabel *m_headline = nullptr;
  QLabel *m_detail = nullptr;
  QHBoxLayout *m_chipLayout = nullptr;
  QHBoxLayout *m_accessoryLayout = nullptr;
  QGridLayout *m_grid = nullptr;
  QWidget *m_actionBox = nullptr;
  QHBoxLayout *m_actionLayout = nullptr;
  bool m_actionsBelow = false;
  QList<StatusChip *> m_chips;
  StatusChip::Kind m_kind = StatusChip::Kind::Neutral;
};
