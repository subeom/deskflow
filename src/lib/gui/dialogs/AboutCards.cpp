/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutCards.h"

#include "CardBuilders.h"
#include "CardPage.h"
#include "gui/style/DesignTokens.h"
#include "gui/widgets/SettingCard.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>

namespace deskflow::gui {

bool applyAboutCards(QDialog *dialog)
{
  using cards::find;
  auto *box = qobject_cast<QBoxLayout *>(dialog->layout());
  auto *header = find<QWidget>(dialog, "frameLogo");
  auto *description = find<QLabel>(dialog, "lblDescription");
  auto *copyright = find<QLabel>(dialog, "lblCopyright");
  auto *oldScroll = find<QScrollArea>(dialog, "scrollArea");
  auto *icons = find<QLabel>(dialog, "label_2");
  auto *group = find<QGroupBox>(dialog, "groupBox");
  auto *developers = find<QLabel>(dialog, "lblImportantDevs");
  auto *close = find<QPushButton>(dialog, "btnOk");
  if (!box || !header || !description || !copyright || !oldScroll || !icons || !group || !developers || !close)
    return false;

  const auto oldMinimum = dialog->layout()->totalMinimumSize();

  // What the program is, who it is by, and where its icons are from
  auto *about = new SettingCard;
  about->setAccessibleName(description->text());
  auto *aboutText = new QWidget;
  auto *aboutLayout = new QVBoxLayout(aboutText);
  aboutLayout->setContentsMargins(0, 0, 0, 0);
  aboutLayout->setSpacing(style::metrics::spaceSm);
  description->setWordWrap(true);
  copyright->setWordWrap(true);
  aboutLayout->addWidget(description);
  aboutLayout->addWidget(copyright);
  aboutLayout->addWidget(icons);
  // the icons of the new look: the MIT licence of Phosphor Icons asks that its notice goes with them (it is in the
  // repository, in icons/phosphor/LICENSE), and the person who has the program can see where they are from
  auto *phosphor = new QLabel(
      QCoreApplication::translate(
          "AboutCards",
          "Icons of the new look from <a href=\"https://phosphoricons.com\">Phosphor Icons</a> (MIT license)"
      )
  );
  phosphor->setObjectName(QStringLiteral("lblPhosphorCredit"));
  phosphor->setTextFormat(Qt::RichText);
  phosphor->setOpenExternalLinks(true);
  phosphor->setWordWrap(true);
  aboutLayout->addWidget(phosphor);
  about->addContent(aboutText);

  // The people that the program is thanks to
  auto *credits = new SettingCard(group->title());
  developers->setTextInteractionFlags(Qt::TextSelectableByMouse);
  credits->addContent(developers);

  // the old scroll area and its group are empty now, and the spacer between the parts has nothing to separate
  oldScroll->hide();
  group->hide();
  for (int i = box->count() - 1; i >= 0; --i) {
    if (box->itemAt(i)->spacerItem())
      delete box->takeAt(i);
  }
  cards::assemblePage(dialog, {about, credits}, close, oldMinimum, header);
  return true;
}

} // namespace deskflow::gui
