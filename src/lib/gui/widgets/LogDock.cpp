/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogDock.h"
#include "LogWidget.h"
#include "SearchWidget.h"

#include "gui/style/Appearance.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/FontPolicy.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/PanelWidget.h"
#include "gui/style/TextSize.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LogDock::LogDock(QWidget *parent)
    : QDockWidget(tr("Log"), parent),
      m_textLog{new LogWidget(this)},
      m_btnClose{new QPushButton(this)},
      m_btnFloat{new QPushButton(this)},
      m_lblTitle{new QLabel(tr("Log"), this)},
      m_searchWidget{new SearchWidget(this)}
{
  connect(m_searchWidget, &SearchWidget::findNext, m_textLog, &LogWidget::findNext);
  connect(m_searchWidget, &SearchWidget::findPrevious, m_textLog, &LogWidget::findPrevious);

  const auto iconSize = QSize(fontMetrics().height() - 2, fontMetrics().height() - 2);
  const auto maxBtnSize = QSize(fontMetrics().height() + 2, fontMetrics().height() + 2);

  m_btnFloat->setFixedSize(maxBtnSize);
  m_btnFloat->setCheckable(true);
  m_btnFloat->setFlat(true);
  m_btnFloat->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize-pip")));
  m_btnFloat->setIconSize(iconSize);
  m_btnFloat->setToolTip(tr("Detach from window"));
  connect(m_btnFloat, &QPushButton::toggled, this, &LogDock::setFloating);

  m_btnClose->setFixedSize(maxBtnSize);
  m_btnClose->setFlat(true);
  m_btnClose->setIcon(QIcon::fromTheme(QStringLiteral("view-close")));
  m_btnClose->setIconSize(iconSize);
  m_btnClose->setToolTip(tr("Close Log"));
  connect(m_btnClose, &QPushButton::clicked, this, &QDockWidget::hide);

  m_card = deskflow::gui::style::isDeskflowStyleInstalled();
  if (m_card)
    buildCard();
  else
    buildTitleBar();

  setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
  setAllowedAreas(Qt::BottomDockWidgetArea);
}

void LogDock::buildTitleBar()
{
  auto titleWidget = new QWidget(this);
  titleWidget->installEventFilter(this);

  auto titleLayout = new QHBoxLayout(titleWidget);
  titleLayout->addWidget(m_lblTitle, Qt::AlignLeft | Qt::AlignVCenter);
  titleLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
  titleLayout->addWidget(m_searchWidget, Qt::AlignRight | Qt::AlignVCenter);
  titleLayout->addWidget(m_btnFloat, Qt::AlignRight | Qt::AlignVCenter);
  titleLayout->addWidget(m_btnClose, Qt::AlignRight | Qt::AlignVCenter);
  setTitleBarWidget(titleWidget);

  auto bodyWidget = new QWidget(this);
  auto bodyLayout = new QVBoxLayout(bodyWidget);
  bodyLayout->addWidget(m_textLog);
  setWidget(bodyWidget);
}

void LogDock::buildCard()
{
  using namespace deskflow::gui::style;

  // the title bar of the dock is empty: the card has the header
  auto *emptyTitle = new QWidget(this);
  emptyTitle->setFixedHeight(0);
  emptyTitle->installEventFilter(this);
  setTitleBarWidget(emptyTitle);

  auto *panel = new PanelWidget(PanelWidget::Density::Dense, this);
  panel->setElevated(false);
  panel->setPadding(metrics::spaceMd);

  // the title and the buttons follow the size of text
  const auto sizeHeader = [this] {
    auto titleFont = scaleFont(QApplication::font(this), qreal(metrics::fontSubtitle) / metrics::fontBody);
    titleFont.setWeight(fontWeight(QFont::DemiBold));
    m_lblTitle->setFont(titleFont);
    for (auto *button : {m_btnFloat, m_btnClose}) {
      button->setFixedSize(px(28), px(28));
      button->setIconSize(QSize(px(18), px(18)));
    }
  };
  sizeHeader();
  onTextSizeChanged(this, sizeHeader);
  updateButtonIcons();

  auto *header = new QHBoxLayout;
  header->setContentsMargins(0, 0, 0, 0);
  header->setSpacing(metrics::spaceSm);
  header->addWidget(m_lblTitle, 0, Qt::AlignVCenter);
  header->addStretch(1);
  header->addWidget(m_searchWidget, 0, Qt::AlignVCenter);
  header->addWidget(m_btnFloat, 0, Qt::AlignVCenter);
  header->addWidget(m_btnClose, 0, Qt::AlignVCenter);

  auto *inside = new QVBoxLayout(panel);
  inside->setSpacing(metrics::spaceSm);
  inside->addLayout(header);
  inside->addWidget(m_textLog, 1);
  m_textLog->useCardLook(tr("Log"));
  m_btnFloat->setAccessibleName(m_btnFloat->toolTip());
  m_btnClose->setAccessibleName(m_btnClose->toolTip());

  // the card has the room of the window around it, as the cards of the page above have
  auto *body = new QWidget(this);
  auto *bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(metrics::spaceMd, 0, metrics::spaceMd, metrics::spaceMd);
  bodyLayout->addWidget(panel);
  setWidget(body);
}

void LogDock::updateButtonIcons()
{
  if (!m_card)
    return;
  using deskflow::gui::style::icon;
  // a dock that has just been made is not floating (its flags are those of a window until the main window takes it, so what
  // isFloating() says at this point is not what it is)
  m_btnClose->setIcon(icon(QStringLiteral("x")));
  m_btnFloat->setIcon(icon(QStringLiteral("arrow-square-out")));
}

void LogDock::appendLine(const QString &msg)
{
  m_textLog->appendLine(msg);
}

void LogDock::setFloating(bool floating)
{
  if (floating) {
    m_btnFloat->setToolTip(tr("Attach to window"));
    m_btnFloat->setIcon(QIcon::fromTheme(QStringLiteral("window-restore-pip")));
    setWindowFlags(Qt::Dialog);
  } else {
    m_btnFloat->setToolTip(tr("Detach from window"));
    m_btnFloat->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize-pip")));
    setWindowFlags(Qt::Widget);
  }
  if (m_card) {
    // the picture and the name of the button say what it does now (isFloating is what the flags that are set make it)
    m_btnFloat->setIcon(deskflow::gui::style::icon(
        floating ? QStringLiteral("arrow-u-up-left") : QStringLiteral("arrow-square-out")
    ));
    m_btnFloat->setAccessibleName(m_btnFloat->toolTip());
  }
  m_lblTitle->setVisible(!floating);
  m_btnClose->setVisible(!floating);
  show();
}

bool LogDock::eventFilter(QObject *watched, QEvent *event)
{
  // Filter out doubleclick on the titlebar, we only want the dock to float if the user users the button on the dock
  if (watched == titleBarWidget() && event->type() == QEvent::MouseButtonDblClick)
    return true;
  return false;
}

void LogDock::changeEvent(QEvent *e)
{
  QDockWidget::changeEvent(e);
  if (e->type() != QEvent::LanguageChange)
    return;

  setWindowTitle(tr("Log"));
  m_lblTitle->setText(tr("Log"));
  m_btnFloat->setToolTip(tr("Detach from window"));
  m_btnClose->setToolTip(tr("Close Log"));
  if (isFloating())
    m_btnFloat->setToolTip(tr("Attach to window"));
  else
    m_btnFloat->setToolTip(tr("Detach from window"));
  if (m_card) {
    m_btnFloat->setAccessibleName(m_btnFloat->toolTip());
    m_btnClose->setAccessibleName(m_btnClose->toolTip());
    m_textLog->setLogName(tr("Log"));
  }
}
