/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/PanelWidget.h"

class QLabel;
class QPushButton;

namespace deskflow::gui::settings {

/*!
 * \brief The line above the pages that says that sharing has to be restarted for a changed setting to be used, with the
 * button that does it.
 *
 * It is shown by whoever knows that a setting that the core reads was changed while the core runs (the window), and
 * hidden by it when the core was restarted or stopped. It does not restart anything itself: it asks (restartRequested).
 * A screen reader finds an alert, so the line is read when it comes.
 */
class RestartBanner : public deskflow::gui::style::PanelWidget
{
  Q_OBJECT

public:
  explicit RestartBanner(QWidget *parent = nullptr);

  //! The button that asks for the restart
  QPushButton *button() const;
  QString text() const;
  //! Shows the banner, or hides it
  void setPending(bool pending);
  bool isPending() const;

  void retranslate();

Q_SIGNALS:
  void restartRequested();

private:
  void applyLook();

  QLabel *m_icon = nullptr;
  QLabel *m_text = nullptr;
  QPushButton *m_button = nullptr;
};

} // namespace deskflow::gui::settings
