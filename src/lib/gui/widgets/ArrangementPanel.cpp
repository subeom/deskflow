/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementPanel.h"

#include "gui/config/ServerConfig.h"
#include "gui/dialogs/LayoutProfilesDialog.h"
#include "gui/layout/LayoutMemoryService.h"
#include "gui/layout/ServerConfigLayout.h"
#include "gui/style/Appearance.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"
#include "gui/widgets/ArrangementCanvas.h"
#include "gui/widgets/LayoutNoticeBar.h"

#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
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
      m_chkRemember(new QCheckBox(tr("Remember the arrangement for each set of displays"), this)),
      m_btnLayouts(new QPushButton(tr("Remembered layouts..."), this)),
      m_noticeBar(new LayoutNoticeBar(this)),
      m_btnClassicGrid(new QPushButton(tr("Use the classic grid"), this)),
      m_btnFit(new QPushButton(tr("Zoom to fit"), this)),
      m_btnReset(new QPushButton(tr("Reset from classic grid"), this)),
      m_btnMonitors(new QPushButton(tr("Edit monitors..."), this)),
      m_btnEdit(new QPushButton(tr("Edit computer..."), this)),
      m_btnRemove(new QPushButton(tr("Remove computer"), this)),
      m_btnAdd(new QPushButton(tr("Add computer..."), this))
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
  m_btnMonitors->setObjectName(QStringLiteral("btnArrangementMonitors"));
  m_btnMonitors->setToolTip(
      tr("Draw the monitors of the selected computer with the mouse: their sizes, their places, and their display "
         "scales.")
  );
  m_btnRemove->setObjectName(QStringLiteral("btnArrangementRemove"));
  m_btnAdd->setObjectName(QStringLiteral("btnArrangementAdd"));
  m_btnAdd->setToolTip(tr("Add a computer. It is put beside the others, where nothing touches it, until you drag it."));

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
      tr("Drag a computer by its middle, resize it by its outline; double-click to edit its monitors. Green lines: "
         "where the pointer goes from one to the other.")
  );
  m_lblHint->setToolTip(
      tr("Drag a computer to where its screen is, the way the displays are arranged in the macOS settings: it snaps to "
         "the edges of the others. Drag the outline of a computer to resize it: a computer whose size is not known is "
         "resized freely (Shift keeps its shape); one that has several monitors or whose screen is known is scaled as "
         "a whole, in its own shape, and keeps its pixels. Alt takes the snapping away, Esc cancels. The green lines "
         "are the edges that touch, and where the pointer goes from one computer to the other. Double click a "
         "computer to draw its monitors.")
  );
  useOneLine(m_lblHint);
  useOneLine(m_lblIssues);

  m_btnClassicGrid->setToolTip(
      tr("Go back to the grid of the screens. The arrangement is kept, and is used again when you choose it again.")
  );
  m_btnFit->setToolTip(tr("Show all of the computers, as big as they fit."));
  m_btnReset->setToolTip(tr("Put the computers where the classic grid has them, with the size of their screens."));
  m_btnEdit->setToolTip(tr("The settings of the selected computer, and its monitors and size as numbers."));
  m_btnRemove->setToolTip(tr("Take the selected computer out of the configuration."));

  const bool themed = deskflow::gui::style::isDeskflowStyleInstalled();
  auto *header = new QHBoxLayout;
  header->addWidget(m_lblTitle, 1);
  // the view is a thing of the canvas, not of the computers: one icon by the way back, with the name that it has
  if (themed) {
    m_btnFit->setIcon(deskflow::gui::style::icon(QStringLiteral("arrows-out")));
    m_btnFit->setFlat(true);
    m_btnFit->setText(QString());
    m_btnFit->setAccessibleName(tr("Zoom to fit"));
    header->addWidget(m_btnFit);
  }
  header->addWidget(m_btnClassicGrid);

  m_chkRemember->setObjectName(QStringLiteral("cbRememberArrangements"));
  m_btnLayouts->setObjectName(QStringLiteral("btnRememberedLayouts"));
  // a check box does not wrap its text: what does not fit the width is cut, and is all in the tool tip
  m_chkRemember->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_chkRemember->setToolTip(
      tr("Experimental. The arrangement of the other computers is remembered for each set of displays of this computer "
         "(for example with and without an external monitor), and put back when the displays change. The core is "
         "restarted only when the configuration really changes. It works with this arrangement, in server mode, with "
         "the configuration of this program (not a file of your own), and is saved with Save.")
  );
  m_btnLayouts->setToolTip(tr("The layouts that are remembered: look at them, name them, forget them."));
  auto *memory = new QHBoxLayout;
  memory->addWidget(m_chkRemember, 1);
  memory->addWidget(m_btnLayouts);

  auto *buttons = new QHBoxLayout;
  if (themed) {
    // what is done to the arrangement on the left, what is done to the computer that is selected on the right, and
    // what takes it out of the configuration apart from the others
    using deskflow::gui::style::icon;
    m_btnAdd->setIcon(icon(QStringLiteral("plus")));
    m_btnReset->setIcon(icon(QStringLiteral("arrow-clockwise")));
    m_btnMonitors->setIcon(icon(QStringLiteral("monitor")));
    m_btnEdit->setIcon(icon(QStringLiteral("pencil-simple")));
    m_btnRemove->setIcon(icon(QStringLiteral("trash"), deskflow::gui::style::IconColor::Danger));
    m_btnRemove->setFlat(true);
    // the text of the button that takes a computer out is the colour of what is not undone
    new deskflow::gui::style::LookFollower(m_btnRemove, [this] {
      auto palette = m_btnRemove->palette();
      for (const auto group : {QPalette::Active, QPalette::Inactive})
        palette.setColor(group, QPalette::ButtonText, deskflow::gui::style::currentTokens().colors.danger);
      m_btnRemove->setPalette(palette);
    });
    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setObjectName(QStringLiteral("arrangementToolsDivider"));
    buttons->addWidget(m_btnAdd);
    buttons->addWidget(m_btnReset);
    buttons->addStretch(1);
    buttons->addWidget(m_btnMonitors);
    buttons->addWidget(m_btnEdit);
    buttons->addWidget(divider);
    buttons->addWidget(m_btnRemove);
  } else {
    buttons->addWidget(m_btnFit);
    buttons->addWidget(m_btnReset);
    buttons->addStretch(1);
    buttons->addWidget(m_btnAdd);
    buttons->addWidget(m_btnMonitors);
    buttons->addWidget(m_btnEdit);
    buttons->addWidget(m_btnRemove);
  }

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(header);
  layout->addLayout(memory);
  layout->addWidget(m_noticeBar);
  layout->addWidget(m_canvas, 1);
  layout->addWidget(m_lblHint);
  layout->addWidget(m_lblIssues);
  layout->addLayout(buttons);

  connect(m_chkRemember, &QCheckBox::toggled, this, &ArrangementPanel::rememberArrangementsToggled);
  connect(m_btnLayouts, &QPushButton::clicked, this, &ArrangementPanel::openLayoutsDialog);
  connect(m_noticeBar, &LayoutNoticeBar::undoRequested, this, &ArrangementPanel::undoLayoutChange);
  connect(m_noticeBar, &LayoutNoticeBar::dismissRequested, this, [] {
    if (auto *service = deskflow::gui::layout::LayoutMemoryService::instance())
      service->dismissNotice();
  });
  // what the remembering of layouts did, if the program that opened this has it
  if (auto *service = deskflow::gui::layout::LayoutMemoryService::instance()) {
    m_noticeBar->setNotice(service->notice());
    connect(service, &deskflow::gui::layout::LayoutMemoryService::noticeChanged, this, [this, service] {
      m_noticeBar->setNotice(service->notice());
    });
  }
  m_btnLayouts->setEnabled(deskflow::gui::layout::LayoutMemoryService::instance() != nullptr);
  connect(m_btnClassicGrid, &QPushButton::clicked, this, &ArrangementPanel::useClassicGridRequested);
  connect(m_btnFit, &QPushButton::clicked, m_canvas, &ArrangementCanvas::zoomToFit);
  connect(m_btnReset, &QPushButton::clicked, this, &ArrangementPanel::resetFromClassicGrid);
  connect(m_btnAdd, &QPushButton::clicked, this, &ArrangementPanel::addComputerRequested);
  connect(m_btnMonitors, &QPushButton::clicked, this, [this] { Q_EMIT editMonitorsRequested(m_canvas->selected()); });
  connect(m_btnEdit, &QPushButton::clicked, this, [this] { Q_EMIT editComputerRequested(m_canvas->selected()); });
  connect(m_btnRemove, &QPushButton::clicked, this, [this] { Q_EMIT removeComputerRequested(m_canvas->selected()); });
  connect(m_canvas, &ArrangementCanvas::selectedChanged, this, &ArrangementPanel::updateSelection);
  // a double click draws the monitors of a computer; those of the server are the operating system's, so it has its
  // settings
  connect(m_canvas, &ArrangementCanvas::editRequested, this, [this](const QString &name) {
    const auto &arrangement = m_canvas->arrangement();
    const auto index = arrangement.indexOf(name.toStdString());
    if (index >= 0 && arrangement.computers[static_cast<size_t>(index)].isServer)
      Q_EMIT editComputerRequested(name);
    else
      Q_EMIT editMonitorsRequested(name);
  });
  connect(m_canvas, &ArrangementCanvas::computerScaled, this, [this](const QString &name, double zoom) {
    m_config.setComputerZoom(name, zoom);
    refresh();
    Q_EMIT changed();
  });
  connect(m_canvas, &ArrangementCanvas::computerResized, this, [this](const QString &name, int, int, int w, int h) {
    m_config.setComputerSize(name, w, h);
    refresh();
    Q_EMIT changed();
  });
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
  m_canvas->setAssumedSizes(m_config.assumedSizes());
  m_canvas->setReportedMonitorComputers(m_config.monitorsFromReport());
  QHash<QString, QSize> reported;
  for (auto it = m_config.reportedScreens().cbegin(); it != m_config.reportedScreens().cend(); ++it)
    reported.insert(it.key(), it.value().size());
  m_canvas->setReportedSizes(reported);
  updateIssues();
  updateSelection();
}

bool ArrangementPanel::hasErrors() const
{
  return m_errors;
}

void ArrangementPanel::setAddEnabled(bool enabled)
{
  m_btnAdd->setEnabled(enabled);
}

void ArrangementPanel::setRememberArrangements(bool on)
{
  // the dialog that owns the setting says it: that is not the user ticking it
  const QSignalBlocker blocker(m_chkRemember);
  m_chkRemember->setChecked(on);
}

bool ArrangementPanel::rememberArrangements() const
{
  return m_chkRemember->isChecked();
}

void ArrangementPanel::openLayoutsDialog()
{
  auto *service = deskflow::gui::layout::LayoutMemoryService::instance();
  if (service == nullptr)
    return;
  // what the canvas shows is what is remembered under a name: the copy of the arrangement of this window
  LayoutProfilesDialog dialog(*service, [this] { return deskflow::gui::layout::captureLayout(m_config); }, this);
  dialog.exec();
}

// The change that the remembering of layouts made is taken back in the copy of the arrangement that this window works on:
// the program does not change the arrangement under a window that is open, and what is saved here is what is remembered
void ArrangementPanel::undoLayoutChange()
{
  auto *service = deskflow::gui::layout::LayoutMemoryService::instance();
  if (service == nullptr)
    return;
  const auto before = service->undoLayout();
  if (!before)
    return;
  if (!deskflow::gui::layout::applyLayout(m_config, *before).applied)
    return;
  service->undoTakenByDialog();
  refresh();
  Q_EMIT changed();
}

void ArrangementPanel::updateSelection()
{
  const auto selected = !m_canvas->selected().isEmpty();
  m_btnEdit->setEnabled(selected);
  const auto &arrangement = m_canvas->arrangement();
  const auto index = arrangement.indexOf(m_canvas->selected().toStdString());
  // the monitors of the server are what the operating system says
  m_btnMonitors->setEnabled(selected && index >= 0 && !arrangement.computers[static_cast<size_t>(index)].isServer);
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
  // a computer that has no monitors and no size is drawn with a size that is a guess: the screen that a computer
  // reports is only in the log, which is not always on a level that has it
  for (const auto &computer : m_canvas->arrangement().computers) {
    const auto name = QString::fromStdString(computer.name);
    if (computer.isServer || !m_canvas->assumedSizes().contains(name, Qt::CaseInsensitive))
      continue;
    const auto shape = computer.shape();
    const auto text = tr("%1: size assumed %2 × %3. Set its monitors or its size in Edit computer.")
                          .arg(name)
                          .arg(shape.w)
                          .arg(shape.h);
    lines << tr("Warning: %1").arg(text);
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
  m_config.adoptReportedScreens();
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
