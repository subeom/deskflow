/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SidebarShell.h"

#include "NavRail.h"
#include "gui/style/Appearance.h"
#include "gui/style/LookFollower.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

using namespace deskflow::gui::style;

namespace {

// Dims what is behind the sidebar in its overlay; a click on it folds the sidebar again
class Scrim : public QWidget
{
public:
  explicit Scrim(QWidget *parent) : QWidget(parent)
  {
    setAttribute(Qt::WA_NoSystemBackground);
    new LookFollower(this, [this] { update(); });
  }

  std::function<void()> onPressed;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), currentTokens().colors.scrim);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    event->accept();
    if (onPressed)
      onPressed();
  }
};

} // namespace

SidebarShell::SidebarShell(QWidget *parent) : QWidget(parent), m_rail(new NavRail(this))
{
  setObjectName(QStringLiteral("sidebarShell"));
  auto *scrim = new Scrim(this);
  scrim->setObjectName(QStringLiteral("sidebarScrim"));
  scrim->onPressed = [this] { closeOverlay(); };
  scrim->hide();
  m_scrim = scrim;
  m_rail->raise();
  connect(m_rail, &NavRail::toggleRequested, this, &SidebarShell::toggleRequested);
  connect(m_rail, &NavRail::panelWidthChanged, this, [this] { relayout(); });
  // an entry that is pressed in the overlay is an answer: the sidebar folds
  connect(m_rail, &NavRail::activated, this, [this] { closeOverlay(); });
  m_rail->setExpanded(true, false);
}

NavRail *SidebarShell::rail() const
{
  return m_rail;
}

QWidget *SidebarShell::content() const
{
  return m_content;
}

void SidebarShell::setContent(QWidget *content)
{
  if (m_content == content)
    return;
  delete m_content;
  m_content = content;
  if (m_content) {
    m_content->setParent(this);
    m_content->show();
    m_content->stackUnder(m_scrim);
  }
  relayout();
  updateGeometry();
}

bool SidebarShell::isCollapsed() const
{
  return m_collapsed;
}

void SidebarShell::setCollapsed(bool collapsed)
{
  if (m_collapsed == collapsed)
    return;
  m_collapsed = collapsed;
  applyState(true);
  Q_EMIT collapsedChanged(collapsed);
}

bool SidebarShell::isNarrow() const
{
  return m_narrow;
}

int SidebarShell::autoCollapseWidth() const
{
  return m_autoCollapseWidth;
}

void SidebarShell::setAutoCollapseWidth(int width)
{
  m_autoCollapseWidth = width;
  const bool narrow = this->width() < width;
  if (narrow != m_narrow) {
    m_narrow = narrow;
    if (!narrow)
      m_overlay = false;
    applyState(true);
    Q_EMIT narrowChanged(narrow);
  }
}

bool SidebarShell::isOverlayOpen() const
{
  return m_overlay;
}

bool SidebarShell::wantsExpanded() const
{
  return m_overlay || (!m_collapsed && !m_narrow);
}

bool SidebarShell::isRailExpanded() const
{
  return m_rail->isExpanded();
}

void SidebarShell::openOverlay()
{
  // there is an overlay only when the window is too narrow for the sidebar to be unfolded beside the content
  if (!m_narrow || m_overlay)
    return;
  m_overlay = true;
  applyState(true);
  m_scrim->show();
  m_scrim->raise();
  m_rail->raise();
  relayout();
  // the focus goes in: to the page that is shown
  if (auto *page = m_rail->item(m_rail->currentPage()))
    page->setFocus(Qt::TabFocusReason);
  Q_EMIT overlayChanged(true);
}

void SidebarShell::closeOverlay()
{
  if (!m_overlay)
    return;
  const bool focusInside = m_rail->isAncestorOf(QApplication::focusWidget());
  m_overlay = false;
  m_scrim->hide();
  applyState(true);
  // the focus goes back to the button that opened it
  if (focusInside || !QApplication::focusWidget())
    m_rail->toggleButton()->setFocus(Qt::TabFocusReason);
  Q_EMIT overlayChanged(false);
}

void SidebarShell::toggleRequested()
{
  if (m_narrow) {
    m_overlay ? closeOverlay() : openOverlay();
    return;
  }
  setCollapsed(!m_collapsed);
}

void SidebarShell::applyState(bool animate)
{
  // over the content the sidebar carries text that must not be read through: it is as opaque as a panel of text
  m_rail->setDensity(
      m_overlay ? deskflow::gui::style::PanelWidget::Density::Dense : deskflow::gui::style::PanelWidget::Density::Normal
  );
  m_rail->setSolid(m_overlay);
  m_rail->setExpanded(wantsExpanded(), animate);
  relayout();
}

bool SidebarShell::event(QEvent *event)
{
  // what the content asks for changed: it is placed again, and the window is told
  if (event->type() == QEvent::LayoutRequest) {
    relayout();
    updateGeometry();
  }
  return QWidget::event(event);
}

void SidebarShell::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  const bool narrow = width() < m_autoCollapseWidth;
  if (narrow != m_narrow) {
    m_narrow = narrow;
    if (!narrow && m_overlay) {
      m_overlay = false;
      m_scrim->hide();
      Q_EMIT overlayChanged(false);
    }
    applyState(false);
    Q_EMIT narrowChanged(narrow);
  } else {
    relayout();
  }
}

void SidebarShell::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape && m_overlay) {
    closeOverlay();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void SidebarShell::relayout()
{
  const auto shadow = deskflow::gui::style::PanelWidget::shadowMargins();
  const int railX = kMargin - shadow.left();
  const int railY = kMargin - shadow.top();
  const int railHeight = height() - 2 * kMargin + shadow.top() + shadow.bottom();
  m_rail->setGeometry(railX, railY, m_rail->width(), railHeight);

  // in the overlay the content stays where it is with the rail folded: it is the sidebar that goes over it
  const int panelBesideContent = m_overlay ? NavRail::collapsedPanelWidth() : m_rail->panelWidth();
  const int contentPanelLeft = kMargin + panelBesideContent + kGap;
  const int contentX = contentPanelLeft - shadow.left();
  const int contentY = kMargin - shadow.top();
  const QRect contentRect(
      contentX, contentY, qMax(0, width() - contentX - (kMargin - shadow.right())),
      qMax(0, height() - 2 * kMargin + shadow.top() + shadow.bottom())
  );
  if (m_content)
    m_content->setGeometry(contentRect);
  m_scrim->setGeometry(contentRect);
}

QSize SidebarShell::sizeHint() const
{
  const auto content = m_content ? m_content->sizeHint() : QSize();
  const auto railHint = m_rail->sizeHint();
  return QSize(
      kMargin + NavRail::expandedPanelWidth() + kGap + qMax(0, content.width() - 20) + kMargin,
      qMax(content.height() - 22, railHint.height() - 22) + 2 * kMargin
  );
}

QSize SidebarShell::minimumSizeHint() const
{
  const auto content = m_content ? m_content->minimumSizeHint() : QSize();
  const auto railHint = m_rail->minimumSizeHint();
  // the narrowest it is is with the rail of icons: the sidebar does not ask for room that a narrow window has not
  return QSize(
      kMargin + NavRail::collapsedPanelWidth() + kGap + qMax(0, content.width() - 20) + kMargin,
      qMax(content.height() - 22, railHint.height() - 22) + 2 * kMargin
  );
}
