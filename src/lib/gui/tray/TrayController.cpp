/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayController.h"

#include "TrayMenuBuilder.h"

#include <QMenu>
#include <QSystemTrayIcon>

#ifdef Q_OS_MACOS
#include "gui/OSXHelpers.h"
#endif

namespace deskflow::gui::tray {

TrayController::TrayController(
    QSystemTrayIcon *icon, QMenu *menu, const TrayPlatform &platform, QObject *parent
)
    : QObject(parent),
      m_icon(icon),
      m_menu(menu),
      m_model(platform),
      m_builder(new TrayMenuBuilder(menu, platform, this))
{
  connect(m_builder, &TrayMenuBuilder::commandTriggered, this, &TrayController::commandRequested);
  // a menu that is drawn by Qt in a frame of its own (opt-in, Windows): see TrayMenuBuilder::setRoundedFrame
  if (platform.headerAsWidget && qEnvironmentVariableIsSet("DESKFLOW_TRAY_MENU_ROUNDED"))
    m_builder->setRoundedFrame(true);
}

const TrayMenuSpec &TrayController::spec() const
{
  return m_spec;
}

TrayState TrayController::state() const
{
  return m_spec.state;
}

const TrayIconSpec &TrayController::iconSpec() const
{
  return m_iconSpec;
}

TrayMenuBuilder *TrayController::builder() const
{
  return m_builder;
}

TrayMenuModel &TrayController::model()
{
  return m_model;
}

void TrayController::setInput(const TrayInput &input)
{
  m_input = input;
  m_hasInput = true;
  refresh();
}

void TrayController::setExternalRow(const QString &key, const QString &text)
{
  m_model.setExternalRow(key, text);
  if (m_hasInput)
    refresh();
}

void TrayController::setColoredIcon(bool colored)
{
  if (m_colored == colored)
    return;
  m_colored = colored;
  if (m_hasInput)
    applyIcon();
}

void TrayController::refreshIcon()
{
  if (m_hasInput)
    applyIcon();
}

void TrayController::setTone(std::optional<TrayTone> tone)
{
  if (m_tone == tone)
    return;
  m_tone = tone;
  if (m_hasInput)
    applyIcon();
}

void TrayController::refresh()
{
  const auto spec = m_model.build(m_input);
  if (spec != m_spec) {
    m_spec = spec;
    m_builder->apply(m_spec);
#ifdef Q_OS_MACOS
    // macOS 27 does not draw the pictures of the rows unless it is asked to
    showOSXMenuImages(m_menu);
#endif
  }
  // the tool tip is only set when it is another (macOS 27.2 leaves a tool tip on the screen that is set again and again)
  if (m_toolTip != m_spec.toolTip) {
    m_toolTip = m_spec.toolTip;
    m_icon->setToolTip(m_toolTip);
  }
  applyIcon();
}

void TrayController::applyIcon()
{
  TrayIconSpec icon;
  icon.state = m_spec.state;
  icon.update = !m_input.updateVersion.isEmpty();
  icon.tone = m_tone.value_or(detectTrayTone());
  // the shapes are coloured on the OS that does not tint the icon itself
  icon.coloredBadge = m_colored && icon.tone != TrayTone::Template;
  if (icon == m_iconSpec && !m_icon->icon().isNull())
    return;
  m_iconSpec = icon;
  // a theme that has an icon of this name (a desktop that tints the symbolic icons) wins over the picture
  if (m_model.platform().themeIcons)
    m_icon->setIcon(QIcon::fromTheme(trayIconName(icon), trayIcon(icon)));
  else
    m_icon->setIcon(trayIcon(icon));
}

} // namespace deskflow::gui::tray
