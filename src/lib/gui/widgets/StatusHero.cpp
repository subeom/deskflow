/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusHero.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"

#include <QAccessible>
#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

using namespace deskflow::gui::style;

namespace {

constexpr int kOrb = 64;
constexpr int kOrbIcon = 30;
constexpr int kPadding = 20;
constexpr int kGap = 16;

QString defaultIcon(StatusChip::Kind kind)
{
  const auto name = StatusChip::iconName(kind);
  return name.isEmpty() ? QStringLiteral("power") : name;
}

QFont scaled(const QWidget *widget, int pixels, QFont::Weight weight)
{
  auto font = scaleFont(QApplication::font(widget), qreal(pixels) / metrics::fontBody);
  font.setWeight(weight);
  return font;
}

} // namespace

// The round badge: a disc in the soft colour of the state with the icon in its strong colour
class StatusOrb : public QWidget
{
public:
  explicit StatusOrb(QWidget *parent) : QWidget(parent)
  {
    setFixedSize(kOrb, kOrb);
  }

  void setState(StatusChip::Kind kind, const QString &icon)
  {
    m_kind = kind;
    m_icon = icon;
    update();
  }

  QString icon() const
  {
    return m_icon;
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto colors = StatusChip::paletteFor(m_kind, currentTokens());
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto dpr = painter.device()->devicePixelRatioF();
    painter.setPen(Qt::NoPen);
    painter.setBrush(colors.fill);
    painter.drawEllipse(QRectF(rect()));

    const auto weight = hasIcon(m_icon, IconWeight::Fill) ? IconWeight::Fill : IconWeight::Regular;
    const auto pixmap = iconPixmap(m_icon, weight, colors.text, QSize(kOrbIcon, kOrbIcon), dpr);
    painter.drawPixmap(QRect((kOrb - kOrbIcon) / 2, (kOrb - kOrbIcon) / 2, kOrbIcon, kOrbIcon), pixmap);
  }

private:
  StatusChip::Kind m_kind = StatusChip::Kind::Neutral;
  QString m_icon;
};

StatusHero::StatusHero(QWidget *parent)
    : PanelWidget(Density::Normal, parent),
      m_orb(new StatusOrb(this)),
      m_headline(new QLabel(this)),
      m_detail(new QLabel(this)),
      m_chipLayout(new QHBoxLayout)
{
  installAccessibleInterfaces();
  setPadding(kPadding);

  for (auto *label : {m_headline, m_detail}) {
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
  }

  m_chipLayout->setContentsMargins(0, 0, 0, 0);
  m_chipLayout->setSpacing(metrics::spaceSm);
  m_chipLayout->addStretch(1);

  auto *textsBox = new QWidget(this);
  auto *texts = new QVBoxLayout(textsBox);
  texts->setContentsMargins(0, 0, 0, 0);
  texts->setSpacing(metrics::spaceXs);
  texts->addWidget(m_headline);
  texts->addWidget(m_detail);
  texts->addLayout(m_chipLayout);
  // what the window puts beside the state (the address of the server) has a line of its own under the chips
  m_accessoryLayout = new QHBoxLayout;
  m_accessoryLayout->setContentsMargins(0, 0, 0, 0);
  m_accessoryLayout->setSpacing(metrics::spaceSm);
  m_accessoryLayout->addStretch(1);
  texts->addLayout(m_accessoryLayout);

  m_actionBox = new QWidget(this);
  m_actionLayout = new QHBoxLayout(m_actionBox);
  m_actionLayout->setContentsMargins(0, 0, 0, 0);
  m_actionLayout->setSpacing(metrics::spaceSm);
  m_actionBox->hide();

  m_grid = new QGridLayout(this);
  m_grid->setContentsMargins(0, 0, 0, 0);
  m_grid->setHorizontalSpacing(kGap);
  m_grid->setVerticalSpacing(metrics::spaceMd);
  m_grid->addWidget(m_orb, 0, 0, 2, 1, Qt::AlignTop);
  m_grid->addWidget(textsBox, 0, 1);
  m_grid->setColumnStretch(1, 1);
  placeActions();

  m_detail->setVisible(false);
  m_orb->setState(m_kind, defaultIcon(m_kind));
  new LookFollower(this, [this] { applyLook(); });
}

void StatusHero::addAccessory(QWidget *widget)
{
  widget->setParent(this);
  m_accessoryLayout->insertWidget(m_accessoryLayout->count() - 1, widget);
}

void StatusHero::setActions(const QList<QWidget *> &actions)
{
  for (auto *action : actions) {
    action->setParent(m_actionBox);
    m_actionLayout->addWidget(action);
  }
  m_actionBox->setVisible(!actions.isEmpty());
  placeActions();
}

bool StatusHero::actionsAreBelow() const
{
  return m_actionsBelow;
}

void StatusHero::placeActions()
{
  // beside the text in a hero that is wide, and under it when it is not
  m_grid->removeWidget(m_actionBox);
  if (m_actionsBelow)
    m_grid->addWidget(m_actionBox, 1, 1, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
  else
    m_grid->addWidget(m_actionBox, 0, 2, 2, 1, Qt::AlignRight | Qt::AlignVCenter);
}

void StatusHero::resizeEvent(QResizeEvent *event)
{
  PanelWidget::resizeEvent(event);
  const bool below = width() < kWrapWidth;
  if (below != m_actionsBelow) {
    m_actionsBelow = below;
    placeActions();
  }
}

StatusChip::Kind StatusHero::kind() const
{
  return m_kind;
}

void StatusHero::setKind(StatusChip::Kind kind)
{
  m_kind = kind;
  m_orb->setState(m_kind, defaultIcon(m_kind));
}

void StatusHero::setIconName(const QString &name)
{
  m_orb->setState(m_kind, name.isEmpty() ? defaultIcon(m_kind) : name);
}

QString StatusHero::iconName() const
{
  return m_orb->icon();
}

QString StatusHero::headline() const
{
  return m_headline->text();
}

void StatusHero::setHeadline(const QString &headline)
{
  if (m_headline->text() == headline)
    return;
  const bool announce = !m_headline->text().isEmpty() && isVisible();
  m_headline->setText(headline);
  setAccessibleName(headline);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  if (announce && QAccessible::isActive()) {
    QAccessibleAnnouncementEvent event(this, headline);
    event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
    QAccessible::updateAccessibility(&event);
  }
#else
  Q_UNUSED(announce)
#endif
}

QString StatusHero::detail() const
{
  return m_detail->text();
}

void StatusHero::setDetail(const QString &detail)
{
  m_detail->setText(detail);
  m_detail->setVisible(!detail.isEmpty());
  setAccessibleDescription(detail);
}

StatusChip *StatusHero::addChip(const QString &text, StatusChip::Kind kind)
{
  auto *chip = new StatusChip(text, kind, this);
  // the chips are first on the line, then what the window put beside them, then the stretch
  m_chipLayout->insertWidget(int(m_chips.size()), chip);
  m_chips.append(chip);
  return chip;
}

void StatusHero::clearChips()
{
  qDeleteAll(m_chips);
  m_chips.clear();
}

QList<StatusChip *> StatusHero::chips() const
{
  return m_chips;
}

void StatusHero::applyLook()
{
  const auto &c = currentTokens().colors;
  m_headline->setFont(scaled(this, metrics::fontSubtitle + 4, QFont::DemiBold));
  m_detail->setFont(scaled(this, metrics::fontBody, QFont::Normal));

  auto palette = m_headline->palette();
  palette.setColor(QPalette::WindowText, c.text);
  m_headline->setPalette(palette);
  palette.setColor(QPalette::WindowText, c.textSecondary);
  m_detail->setPalette(palette);
  m_orb->update();
}
