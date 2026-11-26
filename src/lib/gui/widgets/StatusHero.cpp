/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusHero.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/FontPolicy.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"
#include "gui/style/TextSize.h"

#include <QAccessible>
#include <QApplication>
#include <QFrame>
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

// the badge follows the size of text
int orb()
{
  return px(kOrb);
}

int orbIcon()
{
  return px(kOrbIcon);
}

QString defaultIcon(StatusChip::Kind kind)
{
  const auto name = StatusChip::iconName(kind);
  return name.isEmpty() ? QStringLiteral("power") : name;
}

QFont scaled(const QWidget *widget, int pixels, QFont::Weight weight)
{
  auto font = scaleFont(QApplication::font(widget), qreal(pixels) / metrics::fontBody);
  font.setWeight(fontWeight(weight));
  return font;
}

} // namespace

// The round badge: a disc in the soft colour of the state with the icon in its strong colour
class StatusOrb : public QWidget
{
public:
  explicit StatusOrb(QWidget *parent) : QWidget(parent)
  {
    new LookFollower(this, [this] { setFixedSize(orb(), orb()); });
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
    const auto pixmap = iconPixmap(m_icon, weight, colors.text, QSize(orbIcon(), orbIcon()), dpr);
    painter.drawPixmap(QRect((orb() - orbIcon()) / 2, (orb() - orbIcon()) / 2, orbIcon(), orbIcon()), pixmap);
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
  // the state is named by its headline and described by its detail: the two texts are not walked to again below it
  markRedundantForScreenReader(m_headline);
  markRedundantForScreenReader(m_detail);
  setPadding(kPadding);

  for (auto *label : {m_headline, m_detail}) {
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
  }

  m_chipLayout->setContentsMargins(0, 0, 0, 0);
  m_chipLayout->setSpacing(metrics::spaceSm);
  m_chipLayout->addStretch(1);

  // a frame and not a plain widget: Qt gives a plain widget its interface for a screen reader before the factory of the
  // style layer is asked, and this container is the one that leaves out the headline and the detail
  auto *textsFrame = new QFrame(this);
  textsFrame->setFrameShape(QFrame::NoFrame);
  m_textsBox = textsFrame;
  markFilteredContainerForScreenReader(m_textsBox);
  auto *texts = new QVBoxLayout(m_textsBox);
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
  m_grid->addWidget(m_textsBox, 0, 1);
  m_grid->setColumnStretch(1, 1);
  placeActions();

  m_detail->setVisible(false);
  m_orb->setState(m_kind, defaultIcon(m_kind));
  new LookFollower(this, [this] { applyLook(); });
}

void StatusHero::addAccessory(QWidget *widget)
{
  widget->setParent(this);
  // a line of text goes on to the next line when the hero is narrow: it does not ask for the width of the whole text
  auto *label = qobject_cast<QLabel *>(widget);
  if (label) {
    label->setWordWrap(true);
    // and it has the width of the line, not the narrow one that a text that wraps asks for when it is alone (which
    // breaks "Suggested IP: 10.0.0.2 (all 4 addresses)" at every space): the room that is left is the text's, not the
    // spacer's
    m_accessoryLayout->setStretch(m_accessoryLayout->count() - 1, 0);
  }
  m_accessoryLayout->insertWidget(m_accessoryLayout->count() - 1, widget, label ? 1 : 0);
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

QSize StatusHero::minimumSizeHint() const
{
  const auto margins = contentsMargins();
  const int text = orb() + kGap + m_textsBox->minimumSizeHint().width();
  const int actions = m_actionBox->isHidden() ? 0 : m_actionBox->sizeHint().width();
  return QSize(margins.left() + margins.right() + qMax(text, actions), PanelWidget::minimumSizeHint().height());
}

void StatusHero::updateActionPlacement()
{
  // beside the text when the text at its narrowest and the buttons both fit in the width, and under it when they do
  // not. What fits depends on the font, so it is asked of the parts: a width that was told would be right for one font
  // only
  const int needed = orb() + kGap + m_textsBox->minimumSizeHint().width() + kGap +
                     (m_actionBox->isHidden() ? 0 : m_actionBox->sizeHint().width());
  const bool below = width() < px(kWrapWidth) || contentsRect().width() < needed;
  if (below != m_actionsBelow) {
    m_actionsBelow = below;
    placeActions();
  }
}

void StatusHero::resizeEvent(QResizeEvent *event)
{
  PanelWidget::resizeEvent(event);
  updateActionPlacement();
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
