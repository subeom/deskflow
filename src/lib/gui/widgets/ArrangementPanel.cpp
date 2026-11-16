/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementPanel.h"

#include "gui/config/ServerConfig.h"
#include "gui/widgets/ArrangementCanvas.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace {

// A line that does not wrap: a window does not grow its height for wrapped text, so it would be squeezed. What does not
// fit the width is cut, and is all in the tool tip.
void useOneLine(QLabel *label)
{
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  label->setWordWrap(false);
}

} // namespace

ArrangementPanel::ArrangementPanel(ServerConfig &config, QWidget *parent)
    : QWidget(parent),
      m_config(config),
      m_canvas(new ArrangementCanvas(this)),
      m_lblTitle(new QLabel(this)),
      m_lblHint(new QLabel(this)),
      m_lblIssues(new QLabel(this)),
      m_btnClassicGrid(new QPushButton(tr("Use the classic grid"), this)),
      m_btnFit(new QPushButton(tr("Zoom to fit"), this)),
      m_btnReset(new QPushButton(tr("Reset from classic grid"), this)),
      m_btnEdit(new QPushButton(tr("Edit computer..."), this)),
      m_btnRemove(new QPushButton(tr("Remove computer"), this))
{
  setObjectName(QStringLiteral("arrangementPanel"));
  m_canvas->setObjectName(QStringLiteral("arrangementCanvas"));
  m_lblTitle->setObjectName(QStringLiteral("lblArrangementTitle"));
  m_lblHint->setObjectName(QStringLiteral("lblArrangementHint"));
  m_lblIssues->setObjectName(QStringLiteral("lblArrangementIssues"));
  m_btnClassicGrid->setObjectName(QStringLiteral("btnUseClassicGrid"));
  m_btnFit->setObjectName(QStringLiteral("btnArrangementFit"));
  m_btnReset->setObjectName(QStringLiteral("btnArrangementReset"));
  m_btnEdit->setObjectName(QStringLiteral("btnArrangementEdit"));
  m_btnRemove->setObjectName(QStringLiteral("btnArrangementRemove"));

  auto title = m_lblTitle->font();
  title.setBold(true);
  m_lblTitle->setFont(title);
  m_lblTitle->setText(tr("Experimental: arrangement of the computers is on"));
  m_lblTitle->setToolTip(
      tr("The links between the computers are made from where their screens touch. To turn the experiment off, use "
         "the classic grid, or untick \"Experimental: multi-monitor aware cursor mapping\" in the Advanced tab.")
  );
  useOneLine(m_lblTitle);

  m_lblHint->setText(
      tr("Drag a computer to where its screen is. Green lines: where the pointer goes from one to the other.")
  );
  m_lblHint->setToolTip(
      tr("Drag a computer to where its screen is, the way the displays are arranged in the macOS settings: it snaps to "
         "the edges of the others. The green lines are the edges that touch, and where the pointer goes from one "
         "computer to the other. Double click a computer to set its monitors, its size and its display scale.")
  );
  useOneLine(m_lblHint);
  useOneLine(m_lblIssues);

  m_btnClassicGrid->setToolTip(
      tr("Go back to the grid of the screens. The arrangement is kept, and is used again when you choose it again.")
  );
  m_btnFit->setToolTip(tr("Show all of the computers, as big as they fit."));
  m_btnReset->setToolTip(tr("Put the computers where the classic grid has them, with the size of their screens."));
  m_btnEdit->setToolTip(tr("The monitors, the size and the display scale of the selected computer."));
  m_btnRemove->setToolTip(tr("Take the selected computer out of the configuration."));

  auto *header = new QHBoxLayout;
  header->addWidget(m_lblTitle, 1);
  header->addWidget(m_btnClassicGrid);

  auto *buttons = new QHBoxLayout;
  buttons->addWidget(m_btnFit);
  buttons->addWidget(m_btnReset);
  buttons->addStretch(1);
  buttons->addWidget(m_btnEdit);
  buttons->addWidget(m_btnRemove);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(header);
  layout->addWidget(m_lblHint);
  layout->addWidget(m_canvas, 1);
  layout->addWidget(m_lblIssues);
  layout->addLayout(buttons);

  connect(m_btnClassicGrid, &QPushButton::clicked, this, &ArrangementPanel::useClassicGridRequested);
  connect(m_btnFit, &QPushButton::clicked, m_canvas, &ArrangementCanvas::zoomToFit);
  connect(m_btnReset, &QPushButton::clicked, this, &ArrangementPanel::resetFromClassicGrid);
  connect(m_btnEdit, &QPushButton::clicked, this, [this] { Q_EMIT editComputerRequested(m_canvas->selected()); });
  connect(m_btnRemove, &QPushButton::clicked, this, [this] { Q_EMIT removeComputerRequested(m_canvas->selected()); });
  connect(m_canvas, &ArrangementCanvas::selectedChanged, this, &ArrangementPanel::updateSelection);
  connect(m_canvas, &ArrangementCanvas::editRequested, this, &ArrangementPanel::editComputerRequested);
  connect(m_canvas, &ArrangementCanvas::arrangementChanged, this, [this] {
    m_config.setArrangement(m_canvas->arrangement());
    updateIssues();
    Q_EMIT changed();
  });

  updateSelection();
}

void ArrangementPanel::refresh()
{
  m_canvas->setArrangement(m_config.arrangement());
  updateIssues();
  updateSelection();
}

bool ArrangementPanel::hasErrors() const
{
  return m_errors;
}

void ArrangementPanel::updateSelection()
{
  const auto selected = !m_canvas->selected().isEmpty();
  m_btnEdit->setEnabled(selected);
  const auto &arrangement = m_canvas->arrangement();
  const auto index = arrangement.indexOf(m_canvas->selected().toStdString());
  // the server is the screen of this computer: it is not taken out
  m_btnRemove->setEnabled(selected && index >= 0 && !arrangement.computers[static_cast<size_t>(index)].isServer);
}

void ArrangementPanel::updateIssues()
{
  m_errors = false;
  QStringList lines;
  QStringList tips;
  for (const auto &issue : m_canvas->issues()) {
    const auto text = ArrangementCanvas::describe(issue);
    const bool error = issue.severity() == deskflow::server::arrangement::Issue::Severity::Error;
    m_errors = m_errors || error;
    lines << (error ? tr("Error: %1") : tr("Warning: %1")).arg(text);
    tips << text;
  }
  // what a computer that is connected says its screen is, when the arrangement has another: the server does not use
  // monitors that are not of it, and a size that is not its size draws it wrong
  for (const auto &computer : m_canvas->arrangement().computers) {
    const auto name = QString::fromStdString(computer.name);
    const auto reported = computer.isServer ? std::nullopt : m_config.reportedScreen(name);
    if (!reported)
      continue;
    const auto shape = computer.shape();
    if (shape.x == reported->x() && shape.y == reported->y() && shape.w == reported->width() &&
        shape.h == reported->height())
      continue;
    const auto text = tr("%1 reports a screen of %2 × %3 at %4,%5, but the arrangement has %6 × %7 at %8,%9: set its "
                         "monitors or its size.")
                          .arg(name)
                          .arg(reported->width())
                          .arg(reported->height())
                          .arg(reported->x())
                          .arg(reported->y())
                          .arg(shape.w)
                          .arg(shape.h)
                          .arg(shape.x)
                          .arg(shape.y);
    lines << tr("Warning: %1").arg(text);
    tips << text;
  }
  m_lblIssues->setText(lines.join(QLatin1Char('\n')));
  m_lblIssues->setToolTip(tips.join(QLatin1Char('\n')));
  m_lblIssues->setVisible(!lines.isEmpty());
  m_btnReset->setEnabled(true);
}

void ArrangementPanel::resetFromClassicGrid()
{
  const auto conversion = m_config.classicGridAsArrangement();
  m_config.setArrangement(conversion.arrangement);
  refresh();
  if (!conversion.notKept.empty()) {
    QStringList lost;
    for (const auto &link : conversion.notKept)
      lost << tr("%1 (%2) to %3")
                  .arg(
                      QString::fromStdString(link.from),
                      QString::fromLatin1(deskflow::server::arrangement::directionName(link.side)),
                      QString::fromStdString(link.to)
                  );
    QMessageBox::information(
        this, tr("Arrangement"),
        tr("These links of the classic grid could not be kept, because the screens do not touch there:\n%1")
            .arg(lost.join(QLatin1Char('\n')))
    );
  }
  Q_EMIT changed();
}
