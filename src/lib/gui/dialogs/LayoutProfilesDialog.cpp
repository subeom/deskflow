/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutProfilesDialog.h"

#include "gui/layout/LayoutMemoryService.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

using deskflow::gui::layout::LayoutMemoryService;
using deskflow::gui::layout::Profile;

namespace {

constexpr int kNameColumn = 0;
constexpr int kUsedColumn = 1;
constexpr int kDisplaysColumn = 2;

// A line that does not wrap: a window does not grow its height for wrapped text, so it would be squeezed. What does not
// fit the width is cut, and is all in the tool tip.
void useOneLine(QLabel *label)
{
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  label->setWordWrap(false);
}

} // namespace

LayoutProfilesDialog::LayoutProfilesDialog(
    LayoutMemoryService &service, std::function<deskflow::gui::layout::Layout()> arrangement, QWidget *parent
)
    : QDialog(parent),
      m_service(service),
      m_arrangement(std::move(arrangement)),
      m_lblInfo(new QLabel(this)),
      m_table(new QTableWidget(0, 3, this)),
      m_btnRename(new QPushButton(tr("Rename..."), this)),
      m_btnDelete(new QPushButton(tr("Forget"), this)),
      m_btnSaveAs(new QPushButton(tr("Save current arrangement as..."), this)),
      m_lblCurrent(new QLabel(this))
{
  setObjectName(QStringLiteral("layoutProfilesDialog"));
  setWindowTitle(tr("Remembered layouts"));
  m_lblInfo->setObjectName(QStringLiteral("lblLayoutProfilesInfo"));
  m_table->setObjectName(QStringLiteral("layoutProfilesTable"));
  m_btnRename->setObjectName(QStringLiteral("btnRenameLayout"));
  m_btnDelete->setObjectName(QStringLiteral("btnDeleteLayout"));
  m_btnSaveAs->setObjectName(QStringLiteral("btnSaveLayoutAs"));
  m_lblCurrent->setObjectName(QStringLiteral("lblCurrentDisplays"));

  useOneLine(m_lblInfo);
  useOneLine(m_lblCurrent);
  m_lblCurrent->setTextInteractionFlags(Qt::TextSelectableByMouse);

  m_table->setHorizontalHeaderLabels({tr("Name"), tr("Last used"), tr("Displays")});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setTextElideMode(Qt::ElideRight);
  m_table->verticalHeader()->hide();
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->setMinimumHeight(m_table->fontMetrics().lineSpacing() * 8);
  m_table->setMinimumWidth(m_table->fontMetrics().averageCharWidth() * 60);

  m_btnRename->setToolTip(tr("Give the selected layout a name of your own."));
  m_btnDelete->setToolTip(tr("Forget the selected layout. It is made again the next time that these displays are used and the arrangement is saved."));
  m_btnSaveAs->setToolTip(
      tr("Remember the arrangement that is shown now for the displays that this computer has now, under a name. If a "
         "layout is remembered for these displays already, it is replaced and gets the name.")
  );

  auto *buttons = new QHBoxLayout;
  buttons->addWidget(m_btnRename);
  buttons->addWidget(m_btnDelete);
  buttons->addStretch(1);
  buttons->addWidget(m_btnSaveAs);

  auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_lblInfo);
  layout->addWidget(m_table, 1);
  layout->addLayout(buttons);
  layout->addWidget(m_lblCurrent);
  layout->addWidget(box);

  connect(m_btnRename, &QPushButton::clicked, this, &LayoutProfilesDialog::askForRename);
  connect(m_btnDelete, &QPushButton::clicked, this, &LayoutProfilesDialog::askForDelete);
  connect(m_btnSaveAs, &QPushButton::clicked, this, &LayoutProfilesDialog::askForSaveAs);
  connect(m_table, &QTableWidget::itemSelectionChanged, this, &LayoutProfilesDialog::updateButtons);
  connect(&m_service, &LayoutMemoryService::profilesChanged, this, &LayoutProfilesDialog::refresh);

  refresh();
}

QString LayoutProfilesDialog::selectedId() const
{
  const auto rows = m_table->selectionModel()->selectedRows();
  if (rows.isEmpty())
    return {};
  return m_table->item(rows.first().row(), kNameColumn)->data(Qt::UserRole).toString();
}

void LayoutProfilesDialog::refresh()
{
  const auto selected = selectedId();
  m_table->setRowCount(0);

  const bool running = m_service.isRunning();
  m_lblInfo->setText(
      running ? tr("One layout is remembered for each set of displays of this computer.")
              : tr("Switched off: tick \"Remember the arrangement for each set of displays\".")
  );
  m_lblInfo->setToolTip(
      tr("The program makes a layout when the arrangement is saved and when the displays change, and puts the one of the "
         "displays that there are back. Here the layouts can be named and forgotten. The feature needs both experiments "
         "(multi-monitor and the arrangement), the server mode and the configuration of this program, not a file.")
  );

  std::vector<Profile> profiles = running ? m_service.store().profiles() : std::vector<Profile>{};
  std::ranges::sort(profiles, [](const Profile &a, const Profile &b) {
    if (a.lastUsedAt != b.lastUsedAt)
      return a.lastUsedAt > b.lastUsedAt;
    return a.id < b.id;
  });
  const auto active = m_service.activeProfileId();
  m_table->setRowCount(static_cast<int>(profiles.size()));
  int row = 0;
  int selectedRow = -1;
  for (const auto &profile : profiles) {
    const bool isActive = profile.id == active;
    auto *name = new QTableWidgetItem(isActive ? tr("%1 (current)").arg(profile.name) : profile.name);
    name->setData(Qt::UserRole, profile.id);
    name->setToolTip(profile.name);
    if (isActive) {
      auto font = name->font();
      font.setBold(true);
      name->setFont(font);
    }
    const auto used = QDateTime::fromMSecsSinceEpoch(profile.lastUsedAt);
    auto *when = new QTableWidgetItem(QLocale().toString(used, QLocale::ShortFormat));
    when->setToolTip(QLocale().toString(used, QLocale::LongFormat));
    auto *displays = new QTableWidgetItem(profile.signature.summary());
    displays->setToolTip(profile.signature.summary());
    for (auto *item : {name, when, displays})
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, kNameColumn, name);
    m_table->setItem(row, kUsedColumn, when);
    m_table->setItem(row, kDisplaysColumn, displays);
    if (profile.id == selected)
      selectedRow = row;
    ++row;
  }
  m_table->resizeColumnToContents(kNameColumn);
  m_table->resizeColumnToContents(kUsedColumn);
  if (selectedRow >= 0)
    m_table->selectRow(selectedRow);

  const auto current = m_service.currentDisplays();
  m_lblCurrent->setText(tr("Current displays: %1").arg(current));
  m_lblCurrent->setToolTip(tr("Current displays: %1").arg(current));
  updateButtons();
}

void LayoutProfilesDialog::updateButtons()
{
  const bool running = m_service.isRunning();
  const bool selected = !selectedId().isEmpty();
  m_btnRename->setEnabled(running && selected);
  m_btnDelete->setEnabled(running && selected);
  m_btnSaveAs->setEnabled(running && m_arrangement != nullptr);
}

bool LayoutProfilesDialog::renameSelected(const QString &name)
{
  const auto id = selectedId();
  if (id.isEmpty() || !m_service.renameProfile(id, name))
    return false;
  refresh();
  return true;
}

bool LayoutProfilesDialog::deleteSelected()
{
  const auto id = selectedId();
  if (id.isEmpty() || !m_service.removeProfile(id))
    return false;
  refresh();
  return true;
}

bool LayoutProfilesDialog::saveCurrentAs(const QString &name)
{
  if (!m_arrangement)
    return false;
  const auto id = m_service.saveCurrentAs(name, m_arrangement());
  refresh();
  for (int row = 0; row < m_table->rowCount() && !id.isEmpty(); ++row) {
    if (m_table->item(row, kNameColumn)->data(Qt::UserRole).toString() == id)
      m_table->selectRow(row);
  }
  return !id.isEmpty();
}

void LayoutProfilesDialog::askForRename()
{
  const auto id = selectedId();
  const auto *profile = m_service.store().find(id);
  if (profile == nullptr)
    return;
  bool accepted = false;
  const auto name = QInputDialog::getText(
      this, tr("Rename layout"), tr("Name:"), QLineEdit::Normal, profile->name, &accepted
  );
  if (accepted && !name.trimmed().isEmpty())
    renameSelected(name);
}

void LayoutProfilesDialog::askForDelete()
{
  const auto *profile = m_service.store().find(selectedId());
  if (profile == nullptr)
    return;
  const auto answer = QMessageBox::question(
      this, tr("Forget layout"), tr("Forget the layout \"%1\"?").arg(profile->name), QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No
  );
  if (answer == QMessageBox::Yes)
    deleteSelected();
}

void LayoutProfilesDialog::askForSaveAs()
{
  bool accepted = false;
  const auto name = QInputDialog::getText(
      this, tr("Save current arrangement"), tr("Name for these displays:"), QLineEdit::Normal, QString(), &accepted
  );
  if (accepted && !name.trimmed().isEmpty())
    saveCurrentAs(name);
}
