/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SectionHeader.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/FontPolicy.h"
#include "gui/style/FontScale.h"
#include "gui/style/LookFollower.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>

using namespace deskflow::gui::style;

SectionHeader::SectionHeader(const QString &title, QWidget *parent)
    : QWidget(parent),
      m_title(new QLabel(withoutMnemonic(title), this)),
      m_layout(new QHBoxLayout(this))
{
  installAccessibleInterfaces();
  // the heading is named by its title: the text of the title is not walked to again below it
  markRedundantForScreenReader(m_title);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(metrics::spaceSm);
  m_title->setTextFormat(Qt::PlainText);
  m_title->setTextInteractionFlags(Qt::NoTextInteraction);
  m_layout->addWidget(m_title);
  m_layout->addStretch(1);
  setAccessibleName(m_title->text());
  new LookFollower(this, [this] { applyLook(); });
}

QString SectionHeader::title() const
{
  return m_title->text();
}

void SectionHeader::setTitle(const QString &title)
{
  // a title of a group box has a mnemonic; a heading has no key to press
  m_title->setText(withoutMnemonic(title));
  setAccessibleName(m_title->text());
}

void SectionHeader::setTrailingWidget(QWidget *widget)
{
  if (m_trailing == widget)
    return;
  delete m_trailing;
  m_trailing = widget;
  if (m_trailing)
    m_layout->addWidget(m_trailing);
}

QWidget *SectionHeader::trailingWidget() const
{
  return m_trailing;
}

void SectionHeader::applyLook()
{
  auto font = scaleFont(QApplication::font(this), qreal(metrics::fontSubtitle) / metrics::fontBody);
  font.setWeight(fontWeight(QFont::DemiBold));
  m_title->setFont(font);

  auto palette = m_title->palette();
  palette.setColor(QPalette::WindowText, currentTokens().colors.text);
  m_title->setPalette(palette);
}
