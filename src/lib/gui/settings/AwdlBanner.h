/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/awdl/AwdlModel.h"
#include "gui/style/PanelWidget.h"

class QLabel;
class QPushButton;

namespace deskflow::gui::settings {

/*!
 * \brief The line above the pages that says that the wireless link of AirDrop and Handoff is on and the latency to the
 * router has the spikes that make the mouse stutter, with the button that turns the link off (it asks for the
 * password).
 *
 * It is shown by whoever knows (the window, from the controller of the option, macOS only) and it asks nothing itself:
 * the button says turnOffRequested. A screen reader finds an alert, so the line is read when it comes.
 */
class AwdlBanner : public deskflow::gui::style::PanelWidget
{
  Q_OBJECT

public:
  explicit AwdlBanner(QWidget *parent = nullptr);

  //! The button that asks for the password
  QPushButton *button() const;
  QString text() const;
  //! Shows the banner, or hides it
  void setPending(bool pending);
  bool isPending() const;
  //! The numbers of the last burst that the line says
  void setStats(const deskflow::awdl::BurstStats &stats);
  //! The dialog of the password is open: the button is off
  void setBusy(bool busy);

  void retranslate();

  /*!
   * The width that the banner has when the window is at its least width. The text wraps, and what Qt takes for the
   * least height of a text that wraps is that of its shortest line, so the window would be made too low for the banner
   * at the least width: the least height that the banner asks for is its height at this width (0: as Qt says).
   */
  void setLeastWidth(int width);
  int leastWidth() const;
  QSize minimumSizeHint() const override;

Q_SIGNALS:
  void turnOffRequested();

private:
  void applyLook();

  QLabel *m_icon = nullptr;
  QLabel *m_text = nullptr;
  QPushButton *m_button = nullptr;
  deskflow::awdl::BurstStats m_stats;
  int m_leastWidth = 0;
};

} // namespace deskflow::gui::settings
