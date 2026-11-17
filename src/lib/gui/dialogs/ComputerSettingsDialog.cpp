/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerSettingsDialog.h"
#include "ui_ComputerSettingsDialog.h"

#include "ComputerSettingsCards.h"
#include "gui/config/Computer.h"
#include "gui/style/Appearance.h"
#include "server/Arrangement.h"
#include "server/MonitorGeometry.h"
#include "validators/AliasValidator.h"
#include "validators/ComputerLineNameValidator.h"
#include "validators/ValidationError.h"

#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>
#include <QTextStream>

#include <cmath>

#include <optional>

#include <common/Settings.h>

using enum ComputerConfig::SwitchCorner;
using enum ComputerConfig::Fix;

namespace {

// the property that the server configuration dialog has to say whether the experiment is on, before that is saved
constexpr auto kExperimentProperty = "experimentalMultiMonitor";

// whether a layout can be set: the experiment is on in the dialog that this one was opened from, or if there is none,
// in the settings
bool experimentIsOn(const QWidget *parent)
{
  for (const QObject *object = parent; object != nullptr; object = object->parent()) {
    if (const auto value = object->property(kExperimentProperty); value.isValid())
      return value.toBool();
  }
  return Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool();
}

QString monitorCount(size_t count)
{
  return count == 1 ? QObject::tr("1 monitor") : QObject::tr("%1 monitors").arg(count);
}

// The status is a line that does not wrap: a window does not grow its height for wrapped text, so it would be squeezed.
// What does not fit the width is cut, and is all in the tool tip.
void showStatus(QLabel *label, const QString &text)
{
  label->setText(text);
  label->setToolTip(text);
}

} // namespace

ComputerSettingsDialog::~ComputerSettingsDialog() = default;

ComputerSettingsDialog::ComputerSettingsDialog(QWidget *parent, Computer *computer, const ComputerList *computers)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ComputerSettingsDialog>()},
      m_computer(computer)
{

  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

  ui->lineNameEdit->setText(m_computer->name());
  ui->chkShareClipboard->setChecked(m_computer->shareClipboard());

  m_monitorLayoutAvailable = experimentIsOn(parent);
  // the screen of the server is what the operating system says: nothing that is typed for it would be used
  const bool monitorsCanBeSet = m_monitorLayoutAvailable && !m_computer->isServer();
  const auto fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  ui->textMonitorLayout->setFont(fixedFont);
  // three lines of the font of the text, whatever it is, so that a layout of a few monitors is seen as it is; the field
  // grows with what is in it
  ui->textMonitorLayout->setMinimumHeight(
      ui->textMonitorLayout->fontMetrics().lineSpacing() * 3 + 2 * ui->textMonitorLayout->frameWidth() +
      qRound(ui->textMonitorLayout->document()->documentMargin() * 2)
  );
  ui->textMonitorLayout->setPlainText(m_computer->monitorLayout());
  ui->textMonitorLayout->setEnabled(monitorsCanBeSet);
  ui->btnImportMonitorLayout->setEnabled(monitorsCanBeSet);
  updateMonitorLayoutStatus();

  // the size of the screen when there are no monitors, and the display scale: what the arrangement of the computers has
  // to know of a computer
  auto *arrangement = new QGridLayout;
  m_sbArrangementWidth = new QSpinBox(this);
  m_sbArrangementHeight = new QSpinBox(this);
  m_sbDisplayScale = new QSpinBox(this);
  m_sbArrangementWidth->setObjectName(QStringLiteral("sbArrangementWidth"));
  m_sbArrangementHeight->setObjectName(QStringLiteral("sbArrangementHeight"));
  m_sbDisplayScale->setObjectName(QStringLiteral("sbDisplayScale"));
  for (auto *box : {m_sbArrangementWidth, m_sbArrangementHeight}) {
    box->setRange(0, 32767);
    box->setSpecialValueText(tr("not said"));
    box->setSuffix(tr(" px"));
  }
  m_sbArrangementWidth->setValue(m_computer->arrangementWidth());
  m_sbArrangementHeight->setValue(m_computer->arrangementHeight());
  m_sbDisplayScale->setRange(25, 500);
  m_sbDisplayScale->setSuffix(QStringLiteral(" %"));
  m_sbDisplayScale->setValue(static_cast<int>(std::lround(100.0 / m_computer->arrangementZoom())));
  // what the spin box shows, which is not the scale of a zoom that it cannot show (25 to 500 %)
  m_scaleShown = m_sbDisplayScale->value();
  const auto sizeTip = tr(
      "The size of the screen of this computer in its own pixels, for the arrangement of the computers, when you have "
      "not set monitors above. Not said: 1920 x 1080."
  );
  const auto scaleTip =
      tr("The display scale of this computer: 100 % on a Mac (its units are already points), the scale of Windows "
         "(150 % for a screen that is drawn 1.5 times as big). The arrangement shows it at its size on the screen, "
         "which is its pixels divided by this.");
  auto *lblSize = new QLabel(tr("Screen size (no monitors above):"), this);
  auto *lblTimes = new QLabel(QStringLiteral("×"), this);
  auto *lblScale = new QLabel(tr("Display scale:"), this);
  // named for the new look, which lays the controls out in rows
  lblSize->setObjectName(QStringLiteral("lblArrangementSize"));
  lblTimes->setObjectName(QStringLiteral("lblArrangementTimes"));
  lblScale->setObjectName(QStringLiteral("lblDisplayScale"));
  lblSize->setBuddy(m_sbArrangementWidth);
  lblScale->setBuddy(m_sbDisplayScale);
  for (QWidget *widget :
       {static_cast<QWidget *>(lblSize), static_cast<QWidget *>(m_sbArrangementWidth),
        static_cast<QWidget *>(m_sbArrangementHeight)})
    widget->setToolTip(sizeTip);
  for (QWidget *widget : {static_cast<QWidget *>(lblScale), static_cast<QWidget *>(m_sbDisplayScale)})
    widget->setToolTip(scaleTip);
  arrangement->addWidget(lblSize, 0, 0);
  arrangement->addWidget(m_sbArrangementWidth, 0, 1);
  arrangement->addWidget(lblTimes, 0, 2);
  arrangement->addWidget(m_sbArrangementHeight, 0, 3);
  arrangement->addWidget(lblScale, 1, 0);
  arrangement->addWidget(m_sbDisplayScale, 1, 1);
  arrangement->setColumnStretch(4, 1);
  ui->layoutMonitorLayout->addLayout(arrangement);
  for (auto *box : {m_sbArrangementWidth, m_sbArrangementHeight})
    box->setEnabled(monitorsCanBeSet);
  m_sbDisplayScale->setEnabled(m_monitorLayoutAvailable);

  const auto valNameError = new validators::ValidationError(this, ui->lblNameError);
  const auto valName = new validators::ComputerLineNameValidator(ui->lineNameEdit, valNameError, computers);
  ui->lineNameEdit->setValidator(valName);

  const auto valAliasError = new validators::ValidationError(this, ui->lblAliasError);
  const auto valAlias = new validators::AliasValidator(ui->lineAddAlias, valAliasError);
  ui->lineAddAlias->setValidator(valAlias);

  for (int i = 0; i < m_computer->aliases().count(); i++)
    new QListWidgetItem(m_computer->aliases()[i], ui->listAliases);

  ui->comboShift->setCurrentIndex(m_computer->modifier(static_cast<int>(KeyboardModifier::Shift)));
  ui->comboCtrl->setCurrentIndex(m_computer->modifier(static_cast<int>(KeyboardModifier::Ctrl)));
  ui->comboAlt->setCurrentIndex(m_computer->modifier(static_cast<int>(KeyboardModifier::Alt)));
  ui->comboMeta->setCurrentIndex(m_computer->modifier(static_cast<int>(KeyboardModifier::Meta)));
  ui->comboSuper->setCurrentIndex(m_computer->modifier(static_cast<int>(KeyboardModifier::Super)));
  ui->comboAltGr->setCurrentIndex(m_computer->modifier(static_cast<int>(KeyboardModifier::AltGr)));

  ui->comboF18Key->setCurrentIndex(static_cast<int>(m_computer->f18Action()));
  ui->chkCrossOSInvertX->setChecked(m_computer->crossOSInvertX());
  ui->chkCrossOSInvertY->setChecked(m_computer->crossOSInvertY());
  ui->comboCrossOSInvertScope->setCurrentIndex(static_cast<int>(m_computer->crossOSInvertScope()));
  ui->chkCrossOSSemanticMedia->setChecked(m_computer->crossOSSemanticMedia());

  ui->chkDeadTopLeft->setChecked(m_computer->switchCorner(static_cast<int>(TopLeft)));
  ui->chkDeadTopRight->setChecked(m_computer->switchCorner(static_cast<int>(TopRight)));
  ui->chkDeadBottomLeft->setChecked(m_computer->switchCorner(static_cast<int>(BottomLeft)));
  ui->chkDeadBottomRight->setChecked(m_computer->switchCorner(static_cast<int>(BottomRight)));
  ui->sbSwitchCornerSize->setValue(m_computer->switchCornerSize());

  ui->chkWeakX11Focus->setChecked(Settings::value(Settings::Computer::WeakX11Focus.arg(m_computer->name())).toBool());

  ui->chkFixCapsLock->setChecked(m_computer->fix(CapsLock));
  ui->chkFixNumLock->setChecked(m_computer->fix(NumLock));
  ui->chkFixScrollLock->setChecked(m_computer->fix(ScrollLock));
  ui->chkFixXTest->setChecked(m_computer->fix(XTest));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ComputerSettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ComputerSettingsDialog::reject);
  connect(ui->btnAddAlias, &QPushButton::clicked, this, &ComputerSettingsDialog::addAlias);
  connect(ui->btnRemoveAlias, &QPushButton::clicked, this, &ComputerSettingsDialog::removeAlias);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, &ComputerSettingsDialog::checkNewAliasName);
  connect(ui->listAliases, &QListWidget::itemSelectionChanged, this, &ComputerSettingsDialog::aliasSelected);
  connect(
      ui->textMonitorLayout, &QPlainTextEdit::textChanged, this, &ComputerSettingsDialog::updateMonitorLayoutStatus
  );
  connect(ui->btnImportMonitorLayout, &QPushButton::clicked, this, &ComputerSettingsDialog::chooseMonitorLayoutFile);

  // the new look: the groups are cards, and the dialog is as small as it was
  if (deskflow::gui::style::isDeskflowStyleInstalled())
    deskflow::gui::applyComputerSettingsCards(this, valNameError, valAliasError);
}

void ComputerSettingsDialog::accept()
{
  if (ui->lineNameEdit->text().isEmpty()) {
    QMessageBox::warning(
        this, tr("Computer name is empty"),
        tr("The computer name cannot be empty. "
           "Please either fill in a name or cancel the dialog.")
    );
    return;
  }
  if (!ui->lblNameError->text().isEmpty()) {
    return;
  }

  // a layout that is not valid is not saved: the status line says what is wrong with it. One that cannot be set is left
  // as it is.
  std::optional<QString> monitorLayout;
  if (m_monitorLayoutAvailable && !m_computer->isServer()) {
    const auto text = ui->textMonitorLayout->toPlainText().trimmed();
    if (text.isEmpty()) {
      monitorLayout = QString();
    } else {
      const auto parsed = deskflow::server::MonitorLayout::parse(text.toStdString());
      if (!parsed) {
        ui->textMonitorLayout->setFocus();
        return;
      }
      monitorLayout = QString::fromStdString(parsed.layout->format());
    }
  }

  m_computer->setName(ui->lineNameEdit->text());
  m_computer->setShareClipboard(ui->chkShareClipboard->isChecked());

  m_computer->aliases().clear();

  for (int i = 0; i < ui->listAliases->count(); i++) {
    QString alias(ui->listAliases->item(i)->text());
    if (alias == ui->lineNameEdit->text()) {
      QMessageBox::warning(
          this, tr("Computer name matches alias"),
          tr("The computer name cannot be the same as an alias. "
             "Please either remove the alias or change the computer name.")
      );
      return;
    }
    if (!m_computer->aliases().contains(alias))
      m_computer->addAlias(alias);
  }

  if (monitorLayout) {
    // the scales are those of the monitors that were drawn: numbers that are typed are another layout
    if (*monitorLayout != m_computer->monitorLayout())
      m_computer->setMonitorScales({});
    m_computer->setMonitorLayout(*monitorLayout);
  }
  if (m_monitorLayoutAvailable) {
    if (!m_computer->isServer())
      m_computer->setArrangementSize(m_sbArrangementWidth->value(), m_sbArrangementHeight->value());
    // a scale that was not touched keeps the zoom that was set, which may not be a whole number of percent
    if (m_sbDisplayScale->value() != m_scaleShown) {
      m_computer->setArrangementZoom(deskflow::server::arrangement::zoomForScale(m_sbDisplayScale->value()));
      // the scales of the monitors that were drawn are of the zoom that was: every monitor follows the new one
      m_computer->setMonitorScales({});
    }
  }

  m_computer->setModifier(KeyboardModifier::Shift, ui->comboShift->currentIndex());
  m_computer->setModifier(KeyboardModifier::Ctrl, ui->comboCtrl->currentIndex());
  m_computer->setModifier(KeyboardModifier::Alt, ui->comboAlt->currentIndex());
  m_computer->setModifier(KeyboardModifier::Meta, ui->comboMeta->currentIndex());
  m_computer->setModifier(KeyboardModifier::Super, ui->comboSuper->currentIndex());
  m_computer->setModifier(KeyboardModifier::AltGr, ui->comboAltGr->currentIndex());

  m_computer->setF18Action(
      ui->comboF18Key->currentIndex() == static_cast<int>(F18Action::Hangul) ? F18Action::Hangul
                                                                             : F18Action::PassThrough
  );
  m_computer->setCrossOSInvertX(ui->chkCrossOSInvertX->isChecked());
  m_computer->setCrossOSInvertY(ui->chkCrossOSInvertY->isChecked());
  m_computer->setCrossOSInvertScope(
      ui->comboCrossOSInvertScope->currentIndex() == static_cast<int>(ScrollInvertScope::MouseWheelOnly)
          ? ScrollInvertScope::MouseWheelOnly
          : ScrollInvertScope::All
  );
  m_computer->setCrossOSSemanticMedia(ui->chkCrossOSSemanticMedia->isChecked());

  m_computer->setSwitchCorner(TopLeft, ui->chkDeadTopLeft->isChecked());
  m_computer->setSwitchCorner(TopRight, ui->chkDeadTopRight->isChecked());
  m_computer->setSwitchCorner(BottomLeft, ui->chkDeadBottomLeft->isChecked());
  m_computer->setSwitchCorner(BottomRight, ui->chkDeadBottomRight->isChecked());
  m_computer->setSwitchCornerSize(ui->sbSwitchCornerSize->value());

  m_computer->setFix(CapsLock, ui->chkFixCapsLock->isChecked());
  m_computer->setFix(NumLock, ui->chkFixNumLock->isChecked());
  m_computer->setFix(ScrollLock, ui->chkFixScrollLock->isChecked());
  m_computer->setFix(XTest, ui->chkFixXTest->isChecked());

  Settings::setValue(Settings::Computer::WeakX11Focus.arg(m_computer->name()), ui->chkWeakX11Focus->isChecked());

  QDialog::accept();
}

void ComputerSettingsDialog::addAlias()
{
  if (!ui->lineAddAlias->text().isEmpty() &&
      ui->listAliases->findItems(ui->lineAddAlias->text(), Qt::MatchFixedString).isEmpty()) {
    new QListWidgetItem(ui->lineAddAlias->text(), ui->listAliases);
    ui->lineAddAlias->clear();
  }
}

void ComputerSettingsDialog::removeAlias() const
{
  QList<QListWidgetItem *> items = ui->listAliases->selectedItems();
  qDeleteAll(items);
}

void ComputerSettingsDialog::checkNewAliasName(const QString &text)
{
  ui->btnAddAlias->setEnabled(!text.isEmpty() && ui->lblAliasError->text().isEmpty());
}

void ComputerSettingsDialog::aliasSelected()
{
  ui->btnRemoveAlias->setEnabled(!ui->listAliases->selectedItems().isEmpty());
}

void ComputerSettingsDialog::updateMonitorLayoutStatus()
{
  using deskflow::server::MonitorLayout;

  if (!m_monitorLayoutAvailable) {
    showStatus(
        ui->lblMonitorLayoutStatus,
        tr("Turn on \"Experimental: multi-monitor aware cursor mapping\" in the server's Advanced tab to set a layout.")
    );
    return;
  }

  if (m_computer->isServer()) {
    showStatus(ui->lblMonitorLayoutStatus, tr("The screen of this computer is what the operating system says."));
    return;
  }

  const auto text = ui->textMonitorLayout->toPlainText();
  if (text.trimmed().isEmpty()) {
    showStatus(ui->lblMonitorLayoutStatus, tr("No layout: this computer is mapped as it always was."));
    return;
  }

  const auto parsed = MonitorLayout::parse(text.toStdString());
  if (!parsed) {
    showStatus(ui->lblMonitorLayoutStatus, tr("Not valid: %1").arg(QString::fromStdString(parsed.error)));
    return;
  }

  const auto box = parsed.layout->boundingBox();
  auto status = tr("Valid: %1, bounding box %2,%3 %4x%5.")
                    .arg(monitorCount(parsed.layout->monitors().size()))
                    .arg(box.x)
                    .arg(box.y)
                    .arg(box.w)
                    .arg(box.h);
  if (parsed.layout->isPlainRectangle())
    status += tr(" A rectangle: this changes nothing.");
  if (parsed.duplicatesRemoved > 0)
    status += tr(" %1 that are the same as another are counted once.").arg(parsed.duplicatesRemoved);
  showStatus(ui->lblMonitorLayoutStatus, status);
}

void ComputerSettingsDialog::chooseMonitorLayoutFile()
{
  const auto path = QFileDialog::getOpenFileName(
      this, tr("Import monitor layout"), QString(), tr("Text files (*.txt *.log);;All files (*)")
  );
  if (!path.isEmpty())
    importMonitorLayout(path);
}

bool ComputerSettingsDialog::importMonitorLayout(const QString &path)
{
  using deskflow::server::MonitorLayout;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    showStatus(ui->lblMonitorLayoutStatus, tr("Cannot read %1.").arg(path));
    return false;
  }
  // text that PowerShell wrote may be UTF-16, which the byte order mark tells
  QTextStream stream(&file);
  const auto content = stream.readAll();

  const auto block = MonitorLayout::extractPasteBlock(content.toStdString());
  if (block.empty()) {
    showStatus(
        ui->lblMonitorLayoutStatus,
        tr("This file has no block of monitors to paste (no \"Paste-ready block\", no \"monitor\" line).")
    );
    return false;
  }

  const auto parsed = MonitorLayout::parse(block);
  if (!parsed) {
    // to be put right
    ui->textMonitorLayout->setPlainText(QString::fromStdString(block).trimmed());
    return false;
  }

  // one monitor to a line, to read it
  ui->textMonitorLayout->setPlainText(QString::fromStdString(parsed.layout->format()).replace(QLatin1Char(';'), u'\n'));

  // the scale that Windows says its primary monitor has is how big the computer is drawn in the arrangement (a Mac says
  // pixels per point, which does not change its points)
  if (const auto scale = deskflow::server::arrangement::windowsDisplayScale(content.toStdString()); scale)
    m_sbDisplayScale->setValue(static_cast<int>(std::lround(*scale * 100.0)));
  return true;
}
