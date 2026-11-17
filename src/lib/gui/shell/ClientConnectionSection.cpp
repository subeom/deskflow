/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientConnectionSection.h"

#include "gui/style/LookFollower.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace deskflow::gui {

namespace {

QString withoutColon(QString text)
{
  text = style::withoutMnemonic(text);
  while (text.endsWith(QLatin1Char(':')) || text.endsWith(QLatin1Char(' ')))
    text.chop(1);
  return text;
}

} // namespace

ClientConnectionSection::ClientConnectionSection(
    QLabel *hostLabel, QComboBox *hostname, QPushButton *testButton, QPushButton *configureButton, QWidget *parent
)
    : QWidget(parent),
      m_hostLabel(hostLabel),
      m_hostname(hostname)
{
  setObjectName(QStringLiteral("clientConnectionSection"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_card = new SettingCard(tr("Connection"), this);
  m_card->header()->setTrailingWidget(configureButton);
  layout->addWidget(m_card);

  // the field of the address is the first of a stack, so that something else can stand in its place
  m_addressBox = new QWidget(m_card);
  m_addressBox->setObjectName(QStringLiteral("clientAddressBox"));
  m_addressStack = new QStackedLayout(m_addressBox);
  m_addressStack->setContentsMargins(0, 0, 0, 0);
  m_addressStack->addWidget(hostname);
  m_addressStack->setCurrentIndex(0);
  m_addressBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_addressBox->setMinimumWidth(240);

  m_addressRow = new SettingRow(withoutColon(hostLabel->text()));
  m_addressRow->addControl(m_addressBox);
  m_addressRow->addControl(testButton);
  m_addressRow->setExpandingControls(true);
  m_card->addRow(m_addressRow);
  m_hostLabel->hide();

  m_statusLines = new QWidget(m_card);
  m_statusLines->setObjectName(QStringLiteral("clientStatusLines"));
  m_statusLayout = new QVBoxLayout(m_statusLines);
  m_statusLayout->setContentsMargins(style::metrics::spaceXl, 0, style::metrics::spaceXl, style::metrics::spaceMd);
  m_statusLayout->setSpacing(style::metrics::spaceXs);
  m_statusLines->hide();
  // the lines are the last thing in the card, under the row of the address
  qobject_cast<QVBoxLayout *>(m_card->layout())->addWidget(m_statusLines);

  retranslate();
}

SettingCard *ClientConnectionSection::card() const
{
  return m_card;
}

SettingRow *ClientConnectionSection::addressRow() const
{
  return m_addressRow;
}

QStackedLayout *ClientConnectionSection::addressStack() const
{
  return m_addressStack;
}

void ClientConnectionSection::addStatusLine(QWidget *line)
{
  line->setParent(m_statusLines);
  m_statusLayout->addWidget(line);
  m_statusLines->show();
}

void ClientConnectionSection::removeStatusLine(QWidget *line)
{
  m_statusLayout->removeWidget(line);
  line->setParent(nullptr);
  m_statusLines->setVisible(m_statusLayout->count() > 0);
}

int ClientConnectionSection::statusLineCount() const
{
  return m_statusLayout->count();
}

void ClientConnectionSection::retranslate()
{
  const auto title = withoutColon(m_hostLabel->text());
  m_addressRow->setTitle(title);
  // what a screen reader reads for the field is the title of the row (the stack that holds it is not what is typed in)
  m_hostname->setAccessibleName(title);
  m_addressRow->setToolTip(m_hostname->toolTip());
  // the hint of the field is the help for the field: what to write in it
  if (auto *edit = m_hostname->lineEdit())
    m_addressRow->setDescription(edit->placeholderText());
}

} // namespace deskflow::gui
