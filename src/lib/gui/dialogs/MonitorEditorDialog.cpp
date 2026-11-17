/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorEditorDialog.h"

#include "gui/config/MonitorEditing.h"
#include "gui/style/Appearance.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/Icons.h"
#include "gui/style/PanelWidget.h"
#include "gui/style/StatusColors.h"
#include "gui/widgets/DisclosureBox.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/MonitorEditorWidget.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace deskflow::server;
using namespace deskflow::server::editor;
using deskflow::gui::style::applyStatusColor;
using deskflow::gui::style::Status;

namespace {

void useOneLine(QLabel *label)
{
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
}

} // namespace

MonitorEditorDialog::MonitorEditorDialog(
    QWidget *parent, const QString &computerName, const Layout &drawing, const std::optional<QRect> &reported
)
    : QDialog(parent),
      m_name(computerName),
      m_reported(reported),
      m_history(drawing),
      m_widget(new MonitorEditorWidget(this)),
      m_status(new QLabel(this)),
      m_resolution(new QComboBox(this)),
      m_pixelsW(new QSpinBox(this)),
      m_pixelsH(new QSpinBox(this)),
      m_times(new QLabel(QStringLiteral("×"), this)),
      m_scale(new QComboBox(this)),
      m_rotate(new QPushButton(tr("Rotate"), this)),
      m_makeMain(new QPushButton(tr("Make main"), this)),
      m_add(new QPushButton(tr("Add monitor"), this)),
      m_remove(new QPushButton(tr("Remove monitor"), this)),
      m_undo(new QPushButton(tr("Undo"), this)),
      m_redo(new QPushButton(tr("Redo"), this)),
      m_stretchButton(new QPushButton(tr("Stretch to the reported size"), this)),
      m_drawOwn(new QPushButton(tr("Draw my own instead"), this)),
      m_advanced(new DisclosureBox(tr("Advanced: numbers"), this)),
      m_advancedContent(new QWidget(this)),
      m_numbers(new QPlainTextEdit(this)),
      m_numbersStatus(new QLabel(this)),
      m_import(new QPushButton(tr("Import from diagnostic file..."), this)),
      m_apply(new QPushButton(tr("Use these numbers"), this)),
      m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
  setWindowTitle(tr("Monitors of %1").arg(computerName));
  setSizeGripEnabled(true);

  m_widget->setObjectName(QStringLiteral("monitorEditorWidget"));
  m_status->setObjectName(QStringLiteral("lblMonitorStatus"));
  m_resolution->setObjectName(QStringLiteral("cbResolution"));
  m_pixelsW->setObjectName(QStringLiteral("sbPixelsW"));
  m_pixelsH->setObjectName(QStringLiteral("sbPixelsH"));
  m_scale->setObjectName(QStringLiteral("cbScale"));
  m_rotate->setObjectName(QStringLiteral("btnRotate"));
  m_makeMain->setObjectName(QStringLiteral("btnMakeMain"));
  m_add->setObjectName(QStringLiteral("btnAddMonitor"));
  m_remove->setObjectName(QStringLiteral("btnRemoveMonitor"));
  m_undo->setObjectName(QStringLiteral("btnUndo"));
  m_redo->setObjectName(QStringLiteral("btnRedo"));
  m_stretchButton->setObjectName(QStringLiteral("btnStretchToReported"));
  m_drawOwn->setObjectName(QStringLiteral("btnDrawOwn"));
  m_drawOwn->setVisible(false);
  m_drawOwn->setToolTip(
      tr("Start from the monitors that the computer reports and change them. What you draw is used instead of what the "
         "computer reports.")
  );
  m_advanced->setObjectName(QStringLiteral("groupAdvancedNumbers"));
  m_numbers->setObjectName(QStringLiteral("textNumbers"));
  m_numbersStatus->setObjectName(QStringLiteral("lblNumbersStatus"));
  m_import->setObjectName(QStringLiteral("btnImportNumbers"));
  m_apply->setObjectName(QStringLiteral("btnApplyNumbers"));
  m_buttons->setObjectName(QStringLiteral("monitorEditorButtons"));

  useOneLine(m_status);
  useOneLine(m_numbersStatus);
  m_widget->setToolTip(
      tr("Drag a monitor to move it, and the edges or the corners of the selected one to resize it. R rotates it, "
         "Delete removes it, the arrow keys move it.")
  );

  for (const auto &preset : presets())
    m_resolution->addItem(QString::fromUtf8(preset.name), QPoint(preset.w, preset.h));
  m_resolution->addItem(tr("Other size..."), QPoint(0, 0));
  m_resolution->setToolTip(tr("The resolution of the selected monitor, in its own pixels."));
  for (const auto percent : commonScales())
    m_scale->addItem(QStringLiteral("%1 %").arg(percent));
  m_scale->setEditable(true);
  m_scale->setInsertPolicy(QComboBox::NoInsert);
  m_scale->setToolTip(
      tr("The display scale of the selected monitor, as it is in the display settings of the computer.")
  );
  for (auto *box : {m_pixelsW, m_pixelsH}) {
    box->setRange(320, 16384);
    box->setKeyboardTracking(false);
  }
  m_stretchButton->setToolTip(
      tr("Make the drawing exactly the size that the computer reports, even though its shape is not the same.")
  );
  m_numbers->setToolTip(
      tr("The monitors in the coordinates that the computer reports: x,y,w,h for each, separated by ; or by lines.")
  );
  m_numbers->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_numbers->setMinimumHeight(60);

  auto *numbersButtons = new QHBoxLayout;
  numbersButtons->addWidget(m_import);
  numbersButtons->addWidget(m_apply);
  numbersButtons->addStretch(1);
  auto *numbersLayout = new QVBoxLayout(m_advancedContent);
  numbersLayout->setContentsMargins(0, 0, 0, 0);
  numbersLayout->addWidget(m_numbers);
  numbersLayout->addLayout(numbersButtons);
  numbersLayout->addWidget(m_numbersStatus);
  auto *advancedLayout = new QVBoxLayout(m_advanced);
  advancedLayout->addWidget(m_advancedContent);
  m_advanced->setFlat(true);
  m_advanced->setCheckable(true);
  m_advanced->setChecked(false);
  m_advancedContent->setVisible(false);

  if (deskflow::gui::style::isDeskflowStyleInstalled())
    buildDeskflowLayout();
  else
    buildClassicLayout();

  connect(m_widget, &MonitorEditorWidget::drawingChanged, this, [this](const Layout &layout) {
    m_history.push(layout);
    m_stretch = false;
    m_otherSize = false;
    refresh();
  });
  connect(m_widget, &MonitorEditorWidget::selectionChanged, this, [this] {
    m_otherSize = false;
    refresh();
  });
  connect(m_resolution, &QComboBox::activated, this, [this] { chooseResolution(); });
  for (auto *box : {m_pixelsW, m_pixelsH}) {
    connect(box, &QSpinBox::editingFinished, this, [this] {
      const auto selected = m_widget->selected();
      if (selected >= 0 && m_pixelsW->isEnabled())
        changeTo(
            withResolution(m_history.current(), static_cast<size_t>(selected), m_pixelsW->value(), m_pixelsH->value())
        );
    });
  }
  connect(m_scale, &QComboBox::activated, this, [this] { chooseScale(); });
  connect(m_scale->lineEdit(), &QLineEdit::editingFinished, this, [this] { chooseScale(); });
  connect(m_rotate, &QPushButton::clicked, this, [this] {
    if (m_widget->selected() >= 0)
      changeTo(rotated(m_history.current(), static_cast<size_t>(m_widget->selected())));
  });
  connect(m_makeMain, &QPushButton::clicked, this, [this] {
    if (m_widget->selected() > 0)
      changeTo(withMain(m_history.current(), static_cast<size_t>(m_widget->selected())), 0);
  });
  connect(m_add, &QPushButton::clicked, this, [this] {
    const auto next = added(m_history.current());
    changeTo(next, static_cast<int>(next.monitors.size()) - 1);
  });
  connect(m_remove, &QPushButton::clicked, this, [this] {
    const auto selected = m_widget->selected();
    if (selected < 0)
      return;
    const auto next = removed(m_history.current(), static_cast<size_t>(selected));
    changeTo(next, std::min(selected, static_cast<int>(next.monitors.size()) - 1));
  });
  connect(m_undo, &QPushButton::clicked, this, [this] { undo(); });
  connect(m_redo, &QPushButton::clicked, this, [this] { redo(); });
  new QShortcut(QKeySequence::Undo, this, [this] { undo(); });
  new QShortcut(QKeySequence::Redo, this, [this] { redo(); });
  connect(m_stretchButton, &QPushButton::clicked, this, [this] {
    m_stretch = true;
    refresh();
  });
  connect(m_drawOwn, &QPushButton::clicked, this, [this] {
    m_showingReported = false;
    refresh();
  });
  connect(m_advanced, &QGroupBox::toggled, this, [this](bool on) { m_advancedContent->setVisible(on); });
  connect(m_apply, &QPushButton::clicked, this, [this] { applyNumbers(); });
  connect(m_import, &QPushButton::clicked, this, [this] { importNumbers(); });
  connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
    if (canSave())
      accept();
  });
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  m_widget->setDrawing(drawing);
  m_widget->setSelected(0);
  refresh();
}

// The layout that the editor has always had: a row of the controls of the selected monitor over a row of the buttons
void MonitorEditorDialog::buildClassicLayout()
{
  auto *top = new QHBoxLayout;
  top->addWidget(new QLabel(tr("Monitor"), this));
  top->addWidget(m_resolution, 1);
  top->addWidget(m_pixelsW);
  top->addWidget(m_times);
  top->addWidget(m_pixelsH);
  top->addWidget(new QLabel(tr("Display scale"), this));
  top->addWidget(m_scale);
  top->addWidget(m_rotate);
  top->addWidget(m_makeMain);

  auto *bottom = new QHBoxLayout;
  bottom->addWidget(m_add);
  bottom->addWidget(m_remove);
  bottom->addWidget(m_undo);
  bottom->addWidget(m_redo);
  bottom->addStretch(1);
  bottom->addWidget(m_drawOwn);
  bottom->addWidget(m_stretchButton);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_status);
  layout->addWidget(m_widget, 1);
  layout->addLayout(top);
  layout->addLayout(bottom);
  layout->addWidget(m_advanced);
  layout->addWidget(m_buttons);
}

// The layout of the new look: the actions on the monitors in a bar over the canvas, what the selected monitor is (its
// size and its scale) in a column beside it, then what is said of the drawing, the numbers closed under a disclosure, and
// the hint with the buttons. The same widgets, so that what finds them finds them; the panel is the one of the other
// dialogs of the look
void MonitorEditorDialog::buildDeskflowLayout()
{
  using namespace deskflow::gui::style;

  const struct
  {
    QPushButton *button;
    const char *icon;
  } icons[] = {
      {m_rotate, "arrow-clockwise"}, {m_makeMain, "star"},
      {m_add, "plus"},               {m_remove, "minus"},
      {m_undo, "arrow-u-up-left"},   {m_redo, "arrow-u-up-right"},
  };
  for (const auto &entry : icons) {
    entry.button->setIcon(icon(QString::fromLatin1(entry.icon)));
    entry.button->setFlat(true);
  }

  auto *tools = new QHBoxLayout;
  tools->setSpacing(metrics::spaceXs);
  for (auto *button : {m_rotate, m_makeMain, m_add, m_remove})
    tools->addWidget(button);
  auto *divider = new QFrame(this);
  divider->setFrameShape(QFrame::VLine);
  divider->setFrameShadow(QFrame::Plain);
  divider->setObjectName(QStringLiteral("monitorEditorToolsDivider"));
  tools->addSpacing(metrics::spaceSm);
  tools->addWidget(divider);
  tools->addSpacing(metrics::spaceSm);
  for (auto *button : {m_undo, m_redo})
    tools->addWidget(button);
  tools->addStretch(1);

  // what the selected monitor is, with the name of each control over it
  auto *side = new QVBoxLayout;
  side->setSpacing(metrics::spaceXs);
  side->addWidget(new QLabel(tr("Monitor"), this));
  side->addWidget(m_resolution);
  auto *pixels = new QHBoxLayout;
  pixels->addWidget(m_pixelsW, 1);
  pixels->addWidget(m_times);
  pixels->addWidget(m_pixelsH, 1);
  side->addLayout(pixels);
  side->addSpacing(metrics::spaceSm);
  side->addWidget(new QLabel(tr("Display scale"), this));
  side->addWidget(m_scale);
  side->addSpacing(metrics::spaceMd);
  side->addWidget(m_stretchButton);
  side->addWidget(m_drawOwn);
  side->addStretch(1);
  auto *sideBox = new QWidget(this);
  sideBox->setLayout(side);
  side->setContentsMargins(0, 0, 0, 0);
  sideBox->setFixedWidth(sideBox->sizeHint().width() < 220 ? 220 : sideBox->sizeHint().width());

  auto *middle = new QHBoxLayout;
  middle->setSpacing(metrics::spaceLg);
  middle->addWidget(m_widget, 1);
  middle->addWidget(sideBox);

  // what the canvas is for: the words of its tool tip, which are not only for the pointer
  auto *hint = new InlineHint(m_widget->toolTip(), this);
  hint->setObjectName(QStringLiteral("lblMonitorEditorHint"));
  hint->setWordWrap(true);
  // it takes the width that the buttons leave: a long line must not make the dialog wide
  hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
  auto *footer = new QHBoxLayout;
  footer->addWidget(hint, 1);
  footer->addWidget(m_buttons);

  auto *backdrop = new BackdropWidget(this);
  backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *panel = new PanelWidget(PanelWidget::Density::Dense, backdrop);
  panel->setPadding(metrics::spaceLg);
  auto *content = new QVBoxLayout(panel);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(metrics::spaceMd);
  content->addLayout(tools);
  content->addLayout(middle, 1);
  content->addWidget(m_status);
  content->addWidget(m_advanced);
  content->addLayout(footer);
  auto *frame = new QVBoxLayout(backdrop);
  frame->setContentsMargins(metrics::spaceMd, metrics::spaceMd, metrics::spaceMd, metrics::spaceMd);
  frame->addWidget(panel);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(backdrop);
}

const Layout &MonitorEditorDialog::drawing() const
{
  return m_history.current();
}

Realised MonitorEditorDialog::realised() const
{
  return realise(m_history.current(), deskflow::gui::toMonitorRect(m_reported), m_stretch);
}

bool MonitorEditorDialog::canSave() const
{
  return check(m_history.current()).empty() && realised().fit != Fit::Invalid;
}

void MonitorEditorDialog::changeTo(const Layout &layout, int selected)
{
  m_stretch = false;
  m_otherSize = false;
  m_history.push(layout);
  m_widget->setDrawing(layout);
  if (selected != -2)
    m_widget->setSelected(selected);
  refresh();
}

void MonitorEditorDialog::showReportedMonitors()
{
  m_showingReported = true;
  refresh();
}

void MonitorEditorDialog::updateReportedMode()
{
  m_drawOwn->setVisible(m_showingReported);
  m_widget->setReadOnly(m_showingReported);
  m_widget->setFocusPolicy(m_showingReported ? Qt::NoFocus : Qt::StrongFocus);
  m_numbers->setReadOnly(m_showingReported);
  if (!m_showingReported) {
    // what refresh() and syncControls() do not decide: the buttons of the numbers
    m_import->setEnabled(true);
    m_apply->setEnabled(true);
    return;
  }

  for (QWidget *w :
       {static_cast<QWidget *>(m_resolution), static_cast<QWidget *>(m_pixelsW), static_cast<QWidget *>(m_pixelsH),
        static_cast<QWidget *>(m_scale), static_cast<QWidget *>(m_rotate), static_cast<QWidget *>(m_makeMain),
        static_cast<QWidget *>(m_add), static_cast<QWidget *>(m_remove), static_cast<QWidget *>(m_undo),
        static_cast<QWidget *>(m_redo), static_cast<QWidget *>(m_stretchButton), static_cast<QWidget *>(m_import),
        static_cast<QWidget *>(m_apply)})
    w->setEnabled(false);
  m_status->setText(
      tr("✔ These are the monitors that %1 reports. They are used as they are; draw your own to use others.")
          .arg(m_name)
  );
  applyStatusColor(m_status, Status::Success);
  m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void MonitorEditorDialog::undo()
{
  if (m_showingReported || !m_history.canUndo())
    return;
  m_history.undo();
  m_stretch = false;
  m_otherSize = false;
  m_widget->setDrawing(m_history.current());
  refresh();
}

void MonitorEditorDialog::redo()
{
  if (m_showingReported || !m_history.canRedo())
    return;
  m_history.redo();
  m_stretch = false;
  m_otherSize = false;
  m_widget->setDrawing(m_history.current());
  refresh();
}

QString MonitorEditorDialog::describeProblem(const Problem &problem) const
{
  using enum Problem::Kind;
  switch (problem.kind) {
  case NoMonitors:
    return tr("There is no monitor.");
  case TooMany:
    return tr("At most %1 monitors are supported.").arg(MonitorLayout::kMaxMonitors);
  case BadSize:
    return tr("Monitor %1 has a size that a monitor cannot have.").arg(problem.first + 1);
  case BadScale:
    return tr("Monitor %1 has a display scale that is not between %2 and %3 %.")
        .arg(problem.first + 1)
        .arg(static_cast<int>(kMinScale))
        .arg(static_cast<int>(kMaxScale));
  case Overlap:
    return tr("Monitors %1 and %2 overlap: drag them apart.").arg(problem.first + 1).arg(problem.second + 1);
  case NotConnected:
    return tr("Monitor %1 does not touch the others: monitors have to share part of an edge.").arg(problem.first + 1);
  }
  return {};
}

void MonitorEditorDialog::syncControls()
{
  const auto selected = m_widget->selected();
  const auto &layout = m_history.current();
  const bool has = selected >= 0 && selected < static_cast<int>(layout.monitors.size());
  for (QWidget *w :
       {static_cast<QWidget *>(m_resolution), static_cast<QWidget *>(m_scale), static_cast<QWidget *>(m_rotate)})
    w->setEnabled(has);
  const QSignalBlocker blockResolution(m_resolution);
  const QSignalBlocker blockScale(m_scale);
  const QSignalBlocker blockW(m_pixelsW);
  const QSignalBlocker blockH(m_pixelsH);
  bool other = false;
  if (has) {
    const auto &monitor = layout.monitors[static_cast<size_t>(selected)];
    const QPoint pixels(monitor.pixelsW(), monitor.pixelsH());
    const int found = m_resolution->findData(pixels);
    other = found < 0 || m_otherSize;
    m_resolution->setCurrentIndex(other ? m_resolution->count() - 1 : found);
    m_pixelsW->setValue(std::clamp(pixels.x(), m_pixelsW->minimum(), m_pixelsW->maximum()));
    m_pixelsH->setValue(std::clamp(pixels.y(), m_pixelsH->minimum(), m_pixelsH->maximum()));
    m_scale->setCurrentText(QStringLiteral("%1 %").arg(std::lround(monitor.scale)));
  } else {
    m_resolution->setCurrentIndex(-1);
  }
  other = other || (m_resolution->currentIndex() == m_resolution->count() - 1);
  for (QWidget *w :
       {static_cast<QWidget *>(m_pixelsW), static_cast<QWidget *>(m_times), static_cast<QWidget *>(m_pixelsH)}) {
    w->setVisible(has && other);
    w->setEnabled(has && other);
  }
}

void MonitorEditorDialog::chooseResolution()
{
  const auto selected = m_widget->selected();
  if (selected < 0)
    return;
  const auto pixels = m_resolution->currentData().toPoint();
  if (pixels.x() <= 0) {
    // another size: the combo stays on it and the spin boxes are for it
    m_otherSize = true;
    syncControls();
    return;
  }
  changeTo(withResolution(m_history.current(), static_cast<size_t>(selected), pixels.x(), pixels.y()));
}

void MonitorEditorDialog::chooseScale()
{
  const auto selected = m_widget->selected();
  if (selected < 0)
    return;
  auto text = m_scale->currentText();
  text.remove(QLatin1Char('%'));
  bool ok = false;
  const auto percent = text.trimmed().toDouble(&ok);
  const auto &current = m_history.current().monitors[static_cast<size_t>(selected)];
  if (!ok || percent < kMinScale || percent > kMaxScale) {
    syncControls();
    return;
  }
  if (std::abs(percent - current.scale) < 1e-9)
    return;
  changeTo(withScale(m_history.current(), static_cast<size_t>(selected), percent));
}

void MonitorEditorDialog::refresh()
{
  const auto &layout = m_history.current();
  const auto problems = check(layout);
  const auto made = realised();
  if (made.fit != Fit::Invalid)
    m_numbersZoom = made.zoom;

  QString text;
  auto status = Status::Success;
  const auto reported = m_reported;
  if (!problems.empty()) {
    text = QStringLiteral("✖ ") + describeProblem(problems.front());
    status = Status::Danger;
  } else if (made.fit == Fit::Invalid) {
    text = QStringLiteral("✖ ") + QString::fromStdString(made.error);
    status = Status::Danger;
  } else if (made.fit == Fit::Matches) {
    text = tr("✔ Matches what %1 reports: %2 × %3.").arg(m_name).arg(reported->width()).arg(reported->height());
    if (m_stretch)
      text += QStringLiteral(" ") + tr("The drawing is stretched to it.");
  } else if (made.fit == Fit::Differs) {
    text = tr("⚠ Does not match what %1 reports (%2 × %3): your drawing is %4 × %5, %6 % different in shape. Change "
              "the monitors so that they add up to that shape, or stretch the drawing to it.")
               .arg(m_name)
               .arg(reported->width())
               .arg(reported->height())
               .arg(made.drawingW)
               .arg(made.drawingH)
               .arg(QString::number(made.differsPercent, 'f', 1));
    status = Status::Warning;
  } else {
    text = tr("Not verified yet: %1 has not told the server its screen size. The layout is saved, and checked when it "
              "connects.")
               .arg(m_name);
    status = Status::Warning;
  }
  m_status->setText(text);
  applyStatusColor(m_status, status);

  syncControls();
  const auto selected = m_widget->selected();
  m_remove->setEnabled(selected >= 0 && layout.monitors.size() > 1);
  m_makeMain->setEnabled(selected > 0);
  m_add->setEnabled(layout.monitors.size() < MonitorLayout::kMaxMonitors);
  m_undo->setEnabled(m_history.canUndo());
  m_redo->setEnabled(m_history.canRedo());
  m_stretchButton->setEnabled(reported.has_value() && problems.empty() && made.fit == Fit::Differs);
  m_buttons->button(QDialogButtonBox::Ok)->setEnabled(canSave());

  // the only monitor snaps to the screen that the computer reports, in the units that it is drawn in
  if (reported && layout.monitors.size() == 1) {
    const double k = 100.0 / layout.monitors.front().scale;
    m_widget->setTargetSize(std::make_pair(reported->width() * k, reported->height() * k));
  } else {
    m_widget->setTargetSize(std::nullopt);
  }

  const QSignalBlocker blockNumbers(m_numbers);
  if (made.fit != Fit::Invalid)
    m_numbers->setPlainText(QString::fromStdString(made.text));
  m_numbersStatus->clear();
  updateReportedMode();
}

void MonitorEditorDialog::applyNumbers()
{
  const auto parsed = MonitorLayout::parse(m_numbers->toPlainText().trimmed().toStdString());
  if (!parsed) {
    m_numbersStatus->setText(QString::fromStdString(parsed.error));
    applyStatusColor(m_numbersStatus, Status::Danger);
    return;
  }
  const auto &rects = parsed.layout->monitors();
  const auto box = m_history.current().box();
  int minX = rects.front().x;
  int minY = rects.front().y;
  for (const auto &r : rects) {
    minX = std::min(minX, r.x);
    minY = std::min(minY, r.y);
  }
  const double zoom = m_numbersZoom;
  changeTo(fromStored(rects, zoom, box.x - zoom * minX, box.y - zoom * minY, {}), 0);
}

void MonitorEditorDialog::importNumbers()
{
  const auto path = QFileDialog::getOpenFileName(
      this, tr("Import monitor layout"), QString(), tr("Text files (*.txt *.log);;All files (*)")
  );
  if (path.isEmpty())
    return;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    m_numbersStatus->setText(tr("%1 could not be read.").arg(path));
    applyStatusColor(m_numbersStatus, Status::Danger);
    return;
  }
  const auto text = QString::fromUtf8(file.readAll());
  const auto block = MonitorLayout::extractPasteBlock(text.toStdString());
  m_numbers->setPlainText(block.empty() ? text : QString::fromStdString(block));
  // the display scale that the diagnostic of a Windows PC says is the zoom that the numbers are in
  if (const auto scale = arrangement::windowsDisplayScale(text.toStdString()))
    m_numbersZoom = 1.0 / *scale;
  applyNumbers();
}
