/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "NavRail.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/Contrast.h"
#include "gui/style/FontPolicy.h"
#include "gui/style/FontScale.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PixelSnap.h"
#include "gui/style/TextSize.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QToolTip>

using namespace deskflow::gui::style;

namespace {

constexpr int kSidePadding = 16;
constexpr int kTopPadding = 14;
constexpr int kBottomPadding = 14;
constexpr int kGap = 4;
constexpr int kSectionHeight = 30;
constexpr int kSeparatorHeight = 17;
constexpr int kIconSize = 20;
constexpr int kStatusHeight = 36;
// the least height of an entry when the window is not high enough for them at their height (a screen of 1366x768, a
// window that is small): the rail gives its entries less room before it asks for more than the screen has
constexpr int kCompactItemHeight = 32;

// the sizes that carry text follow the size of text: the height of a row of the rail, its icons, its group titles
int sectionHeight()
{
  return px(kSectionHeight);
}

int separatorHeight()
{
  return px(kSeparatorHeight);
}

int railIconSize()
{
  return px(kIconSize);
}

int statusHeight()
{
  return px(kStatusHeight);
}

QColor textColor(const Tokens &tokens, NavItem::Kind kind, bool current, bool checked)
{
  const auto &c = tokens.colors;
  if (current)
    return c.onAccent;
  if (checked)
    return c.accent;
  return kind == NavItem::Kind::Status ? c.textSecondary : c.text;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

NavItem::NavItem(Kind kind, const QString &id, const QString &iconName, const QString &text, QWidget *parent)
    : QAbstractButton(parent),
      m_kind(kind),
      m_id(id),
      m_iconName(iconName)
{
  installAccessibleInterfaces();
  setText(text);
  setObjectName(QStringLiteral("navItem-%1").arg(id));
  setAccessibleName(text);
  setFocusPolicy(kind == Kind::Status ? Qt::NoFocus : Qt::ClickFocus);
  setCursor(kind == Kind::Status ? Qt::ArrowCursor : Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover);
  new LookFollower(this, [this] { update(); });
}

NavItem::Kind NavItem::kind() const
{
  return m_kind;
}

QString NavItem::id() const
{
  return m_id;
}

QString NavItem::iconName() const
{
  return m_iconName;
}

void NavItem::setIconName(const QString &name)
{
  m_iconName = name;
  update();
}

void NavItem::setIconColor(IconColor color)
{
  m_iconColor = color;
  update();
}

bool NavItem::isCurrent() const
{
  return m_current;
}

void NavItem::setCurrent(bool current)
{
  if (m_current == current)
    return;
  m_current = current;
  update();
}

void NavItem::setRailCollapsed(bool collapsed)
{
  m_collapsed = collapsed;
  setToolTip(collapsed ? text() : QString());
  update();
}

int NavItem::itemHeight()
{
  return px(kHeight);
}

QSize NavItem::sizeHint() const
{
  const auto label = fontMetrics().horizontalAdvance(text());
  const int height = (m_kind == Kind::Status ? statusHeight() : itemHeight()) + 2 * kRing;
  return QSize(2 * kRing + itemHeight() + 8 + label + 12, height);
}

void NavItem::changeEvent(QEvent *event)
{
  QAbstractButton::changeEvent(event);
  if (event->type() == QEvent::EnabledChange || event->type() == QEvent::FontChange)
    update();
  if (event->type() == QEvent::EnabledChange || event->type() == QEvent::ActivationChange)
    update();
}

void NavItem::keyPressEvent(QKeyEvent *event)
{
  switch (event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Down:
  case Qt::Key_Home:
  case Qt::Key_End:
    Q_EMIT navigate(event->key());
    event->accept();
    return;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    click();
    event->accept();
    return;
  default:
    break;
  }
  QAbstractButton::keyPressEvent(event);
}

void NavItem::focusInEvent(QFocusEvent *event)
{
  m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                    event->reason() == Qt::ShortcutFocusReason;
  QAbstractButton::focusInEvent(event);
  // the rail of icons says what the icon is
  if (m_keyboardFocus && m_collapsed)
    QToolTip::showText(mapToGlobal(QPoint(width(), height() / 2)), text(), this);
  update();
}

void NavItem::focusOutEvent(QFocusEvent *event)
{
  m_keyboardFocus = false;
  QAbstractButton::focusOutEvent(event);
  update();
}

void NavItem::paintEvent(QPaintEvent *)
{
  const auto tokens = currentTokens();
  const auto &c = tokens.colors;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto dpr = painter.device()->devicePixelRatioF();

  const bool current = m_kind == Kind::Page && m_current;
  const bool checked = m_kind == Kind::Action && isChecked();
  const bool hover = underMouse() && m_kind != Kind::Status && isEnabled();
  const QRectF capsule = snapRect(QRectF(rect()).adjusted(kRing, kRing, -kRing, -kRing), dpr);
  const qreal radius = qMin(capsule.height() / 2, qreal(22));

  if (!isEnabled())
    painter.setOpacity(0.45);

  painter.setPen(Qt::NoPen);
  if (current) {
    painter.setBrush(c.accent);
    painter.drawRoundedRect(capsule, radius, radius);
  } else if (checked) {
    painter.setBrush(c.accentSoft);
    painter.drawRoundedRect(capsule, radius, radius);
  } else if (hover) {
    auto fill = c.text;
    fill.setAlphaF(0.08);
    painter.setBrush(fill);
    painter.drawRoundedRect(capsule, radius, radius);
  }

  const auto color = textColor(tokens, m_kind, current, checked);
  // the icons keep their place while the rail moves: they are centred in the first square of the capsule
  const QRect iconBox(
      qRound(capsule.left()) + (itemHeight() - railIconSize()) / 2, qRound(capsule.center().y() - railIconSize() / 2.0),
      railIconSize(), railIconSize()
  );
  auto iconTint = color;
  if (m_kind == Kind::Status && !current)
    iconTint = iconColor(m_iconColor, QIcon::Normal, tokens);
  auto weight = IconWeight::Regular;
  if (m_kind == Kind::Page && hasIcon(m_iconName, IconWeight::Duotone))
    weight = IconWeight::Duotone;
  const auto pixmap = iconPixmap(m_iconName, weight, iconTint, QSize(railIconSize(), railIconSize()), dpr);
  painter.drawPixmap(iconBox, pixmap);

  // the label is drawn where it is room for it: the rail that is moving shows as much of it as there is
  const int textLeft = qRound(capsule.left()) + itemHeight() + 4;
  const QRect textBox(
      textLeft, qRound(capsule.top()), qMax(0, qRound(capsule.right()) - textLeft - 10), qRound(capsule.height())
  );
  if (textBox.width() > 6) {
    auto font = this->font();
    if (m_kind == Kind::Status)
      font = scaleFont(font, 0.93);
    if (current)
      font.setWeight(fontWeight(QFont::DemiBold));
    painter.setFont(font);
    painter.setPen(color);
    const auto elided = QFontMetrics(font).elidedText(text(), Qt::ElideRight, textBox.width());
    painter.drawText(textBox, Qt::AlignVCenter | Qt::AlignLeft, elided);
  }

  if (m_keyboardFocus) {
    const auto ring = deviceLineWidth(metrics::focusRingWidth, dpr);
    const auto outer = capsule.adjusted(-1.5, -1.5, 1.5, 1.5).adjusted(ring / 2, ring / 2, -ring / 2, -ring / 2);
    painter.setOpacity(1.0);
    painter.setPen(QPen(c.accent, ring));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(outer, outer.height() / 2, outer.height() / 2);
  }
}

// ---------------------------------------------------------------------------------------------------------------------

NavToggleButton::NavToggleButton(QWidget *parent) : QAbstractButton(parent)
{
  installAccessibleInterfaces();
  setObjectName(QStringLiteral("navToggle"));
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover);
  new LookFollower(this, [this] { update(); });
}

void NavToggleButton::setExpandedState(bool expanded)
{
  m_expanded = expanded;
  update();
}

bool NavToggleButton::isExpandedState() const
{
  return m_expanded;
}

QSize NavToggleButton::sizeHint() const
{
  return QSize(NavItem::itemHeight() + 2 * NavItem::kRing, NavItem::itemHeight() + 2 * NavItem::kRing);
}

void NavToggleButton::focusInEvent(QFocusEvent *event)
{
  m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                    event->reason() == Qt::ShortcutFocusReason;
  QAbstractButton::focusInEvent(event);
  update();
}

void NavToggleButton::focusOutEvent(QFocusEvent *event)
{
  m_keyboardFocus = false;
  QAbstractButton::focusOutEvent(event);
  update();
}

void NavToggleButton::paintEvent(QPaintEvent *)
{
  const auto tokens = currentTokens();
  const auto &c = tokens.colors;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto dpr = painter.device()->devicePixelRatioF();
  const QRectF circle =
      snapRect(QRectF(rect()).adjusted(NavItem::kRing, NavItem::kRing, -NavItem::kRing, -NavItem::kRing), dpr);
  const auto line = deviceLineWidth(1, dpr);

  auto fill = c.surface;
  if (underMouse() || isDown())
    fill = over(c.text, isDown() ? 0.14 : 0.08, c.surface);
  painter.setPen(QPen(c.controlBorder, line));
  painter.setBrush(fill);
  const auto shape = circle.adjusted(line / 2, line / 2, -line / 2, -line / 2);
  painter.drawRoundedRect(shape, shape.height() / 2, shape.height() / 2);

  const auto pixmap = iconPixmap(
      QStringLiteral("sidebar-simple"), IconWeight::Regular, c.text,
      QSize(NavItem::itemHeight() / 2, NavItem::itemHeight() / 2), dpr
  );
  const auto side = NavItem::itemHeight() / 2;
  painter.drawPixmap(
      QRect(qRound(circle.center().x() - side / 2.0), qRound(circle.center().y() - side / 2.0), side, side), pixmap
  );

  if (m_keyboardFocus) {
    const auto ring = deviceLineWidth(metrics::focusRingWidth, dpr);
    const auto outer = circle.adjusted(-1.5, -1.5, 1.5, 1.5).adjusted(ring / 2, ring / 2, -ring / 2, -ring / 2);
    painter.setPen(QPen(c.accent, ring));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(outer, outer.height() / 2, outer.height() / 2);
  }
}

// ---------------------------------------------------------------------------------------------------------------------

NavRail::NavRail(QWidget *parent) : PanelWidget(Density::Normal, parent), m_toggle(new NavToggleButton(this))
{
  installAccessibleInterfaces();
  setObjectName(QStringLiteral("navRail"));
  setPadding(0);
  setAccessibleName(tr("Sidebar"));
  m_panelWidth = expandedPanelWidth();
  setFixedWidth(widgetWidthFor(expandedPanelWidth()));

  m_toggle->setAccessibleName(m_collapseText);
  m_toggle->setToolTip(m_collapseText);
  connect(m_toggle, &QAbstractButton::clicked, this, &NavRail::toggleRequested);

  m_animation.setEasingCurve(QEasingCurve::OutCubic);
  connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
    applyPanelWidth(value.toReal());
  });
  // the widths of the rail follow the size of text, at once
  onTextSizeChanged(this, [this] {
    m_animation.stop();
    applyPanelWidth(m_expanded ? expandedPanelWidth() : collapsedPanelWidth());
    updateGeometry();
  });
}

int NavRail::collapsedPanelWidth()
{
  return px(deskflow::gui::style::metrics::sidebarCollapsed);
}

int NavRail::expandedPanelWidth()
{
  return px(deskflow::gui::style::metrics::sidebarExpanded);
}

int NavRail::widgetWidthFor(int panelWidth)
{
  const auto room = shadowMargins();
  return panelWidth + room.left() + room.right();
}

int NavRail::panelWidth() const
{
  return qRound(m_panelWidth);
}

bool NavRail::isExpanded() const
{
  return m_expanded;
}

NavItem *
NavRail::add(NavItem::Kind kind, const QString &id, const QString &iconName, const QString &text, Region region)
{
  auto *item = new NavItem(kind, id, iconName, text, this);
  item->setRailCollapsed(!m_expanded);
  if (kind == NavItem::Kind::Page) {
    item->setCheckable(false);
    connect(item, &QAbstractButton::clicked, this, [this, id] {
      setCurrentPage(id);
      Q_EMIT pageSelected(id);
      Q_EMIT activated(id);
    });
  } else if (kind == NavItem::Kind::Action) {
    connect(item, &QAbstractButton::clicked, this, [this, id] { Q_EMIT activated(id); });
  }
  connect(item, &NavItem::navigate, this, &NavRail::moveFocus);
  Entry entry;
  entry.item = item;
  entry.region = region;
  m_entries.append(entry);
  item->show();
  if (!m_focusStop && kind != NavItem::Kind::Status)
    setFocusStop(item);
  relayout();
  return item;
}

NavItem *NavRail::addPage(const QString &id, const QString &iconName, const QString &text, Region region)
{
  auto *item = add(NavItem::Kind::Page, id, iconName, text, region);
  // the first page that is added is the page that is shown
  if (m_current.isEmpty())
    setCurrentPage(id);
  return item;
}

NavItem *
NavRail::addPageBefore(const QString &beforeId, const QString &id, const QString &iconName, const QString &text)
{
  auto *item = add(NavItem::Kind::Page, id, iconName, text, Region::Top);
  // the entry is at the end now: it goes to the place of the one that it comes before
  for (int i = 0; i < m_entries.size() - 1; ++i) {
    if (m_entries[i].item && m_entries[i].item->id() == beforeId) {
      m_entries.move(m_entries.size() - 1, i);
      relayout();
      break;
    }
  }
  return item;
}

NavItem *NavRail::addAction(const QString &id, const QString &iconName, const QString &text, Region region)
{
  return add(NavItem::Kind::Action, id, iconName, text, region);
}

NavItem *NavRail::addStatus(const QString &id, const QString &iconName, const QString &text, Region region)
{
  return add(NavItem::Kind::Status, id, iconName, text, region);
}

void NavRail::addSection(const QString &text)
{
  Entry entry;
  entry.section = text;
  m_entries.append(entry);
  relayout();
}

void NavRail::addSeparator()
{
  Entry entry;
  entry.separator = true;
  m_entries.append(entry);
  relayout();
}

void NavRail::setSectionText(int index, const QString &text)
{
  int seen = 0;
  for (auto &entry : m_entries) {
    if (entry.item || entry.separator)
      continue;
    if (seen++ == index) {
      entry.section = text;
      update();
      return;
    }
  }
}

NavItem *NavRail::item(const QString &id) const
{
  for (const auto &entry : m_entries) {
    if (entry.item && entry.item->id() == id)
      return entry.item;
  }
  return nullptr;
}

QList<NavItem *> NavRail::items() const
{
  QList<NavItem *> result;
  for (const auto &entry : m_entries) {
    if (entry.item)
      result.append(entry.item);
  }
  return result;
}

void NavRail::setItemText(const QString &id, const QString &text)
{
  if (auto *found = item(id)) {
    found->setText(text);
    found->setAccessibleName(text);
    found->setRailCollapsed(!m_expanded);
    found->update();
  }
}

void NavRail::setItemVisible(const QString &id, bool visible)
{
  auto *found = item(id);
  if (!found || found->isHidden() == !visible)
    return;
  found->setVisible(visible);
  // the stop of the focus is one that is shown
  if (!visible && m_focusStop == found) {
    m_focusStop = nullptr;
    const auto candidates = focusable();
    setFocusStop(candidates.isEmpty() ? nullptr : candidates.first());
  } else if (visible && !m_focusStop && found->kind() != NavItem::Kind::Status) {
    setFocusStop(found);
  }
  relayout();
}

QString NavRail::currentPage() const
{
  return m_current;
}

void NavRail::setCurrentPage(const QString &id)
{
  m_current = id;
  for (auto *candidate : items())
    candidate->setCurrent(candidate->kind() == NavItem::Kind::Page && candidate->id() == id);
  if (auto *found = item(id); found && found->kind() == NavItem::Kind::Page)
    setFocusStop(found);
}

NavToggleButton *NavRail::toggleButton() const
{
  return m_toggle;
}

void NavRail::setToggleTexts(const QString &collapseText, const QString &expandText)
{
  m_collapseText = collapseText;
  m_expandText = expandText;
  const auto text = m_expanded ? m_collapseText : m_expandText;
  m_toggle->setAccessibleName(text);
  m_toggle->setToolTip(text);
}

void NavRail::setExpanded(bool expanded, bool animate)
{
  const auto target = expanded ? expandedPanelWidth() : collapsedPanelWidth();
  const bool changed = m_expanded != expanded;
  m_expanded = expanded;
  m_toggle->setExpandedState(expanded);
  const auto text = expanded ? m_collapseText : m_expandText;
  m_toggle->setAccessibleName(text);
  m_toggle->setToolTip(text);
  // the label is the tooltip as soon as the rail is on its way to be one of icons
  for (auto *candidate : items())
    candidate->setRailCollapsed(!expanded);

  m_animation.stop();
  const auto duration = motionDuration(currentLook(), metrics::motionSidebar);
  if (!animate || !isVisible() || duration == 0 || !isDeskflowStyleInstalled()) {
    applyPanelWidth(target);
  } else {
    m_animation.setDuration(duration);
    m_animation.setStartValue(m_panelWidth);
    m_animation.setEndValue(qreal(target));
    m_animation.start();
  }
  if (changed)
    Q_EMIT expandedChanged(expanded);
}

void NavRail::applyPanelWidth(qreal width)
{
  const bool resized = qRound(width) != qRound(m_panelWidth);
  m_panelWidth = width;
  setFixedWidth(widgetWidthFor(qRound(width)));
  relayout();
  update();
  if (resized)
    Q_EMIT panelWidthChanged(qRound(width));
}

QList<NavItem *> NavRail::focusable() const
{
  QList<NavItem *> result;
  for (auto *candidate : items()) {
    if (candidate->kind() != NavItem::Kind::Status && !candidate->isHidden() && candidate->isEnabled())
      result.append(candidate);
  }
  return result;
}

void NavRail::setFocusStop(NavItem *stop)
{
  m_focusStop = stop;
  // one stop in the order of the Tab key for all the entries: the others are reached by the arrow keys
  for (auto *candidate : items()) {
    if (candidate->kind() == NavItem::Kind::Status)
      continue;
    candidate->setFocusPolicy(candidate == stop ? Qt::StrongFocus : Qt::ClickFocus);
  }
}

void NavRail::moveFocus(int key)
{
  const auto list = focusable();
  if (list.isEmpty())
    return;
  int index = list.indexOf(qobject_cast<NavItem *>(sender()));
  if (index < 0)
    index = 0;
  switch (key) {
  case Qt::Key_Up:
    index = qMax(0, index - 1);
    break;
  case Qt::Key_Down:
    index = qMin(int(list.size()) - 1, index + 1);
    break;
  case Qt::Key_Home:
    index = 0;
    break;
  case Qt::Key_End:
    index = int(list.size()) - 1;
    break;
  default:
    return;
  }
  setFocusStop(list[index]);
  list[index]->setFocus(Qt::TabFocusReason);
}

// The height of an entry of the rail when the pages and the actions in it are itemHeight high: a line of status is as
// much higher than them as it is at the size that was drawn
int NavRail::entryHeight(const Entry &entry, int itemHeight) const
{
  if (entry.item) {
    if (entry.item->kind() != NavItem::Kind::Status)
      return itemHeight;
    return qRound(qreal(statusHeight()) * itemHeight / NavItem::itemHeight());
  }
  return entry.separator ? separatorHeight() : sectionHeight();
}

int NavRail::gapFor(int itemHeight) const
{
  return itemHeight >= NavItem::itemHeight() ? kGap : qMax(2, qRound(qreal(kGap) * itemHeight / NavItem::itemHeight()));
}

// The height that the entries, the button of the sidebar and the paddings need (the room of the shadow is not in it)
int NavRail::panelHeightFor(int itemHeight) const
{
  const int gap = gapFor(itemHeight);
  int height = kTopPadding + NavItem::itemHeight() + 12 + kBottomPadding;
  for (const auto &entry : m_entries) {
    if (entry.item && entry.item->isHidden())
      continue;
    height += entryHeight(entry, itemHeight) + gap;
  }
  return height;
}

int NavRail::compactItemHeight()
{
  return qMin(NavItem::itemHeight(), px(kCompactItemHeight));
}

// The entries are as high as they are drawn for, and less high when the panel has not the room: down to the compact
// height. The rail is then still all there, and the window does not have to be as high as for the entries at full
// height.
int NavRail::itemHeightFor(int panelHeight) const
{
  for (int height = NavItem::itemHeight(); height > compactItemHeight(); --height) {
    if (panelHeightFor(height) <= panelHeight)
      return height;
  }
  return compactItemHeight();
}

int NavRail::currentItemHeight() const
{
  return itemHeightFor(panelRect().height());
}

void NavRail::relayout()
{
  const auto panel = panelRect();
  const int itemWidth = panel.width() - 2 * kSidePadding + 2 * NavItem::kRing;
  const int x = panel.left() + kSidePadding - NavItem::kRing;
  const int itemHeight = itemHeightFor(panel.height());
  const int gap = gapFor(itemHeight);

  m_toggle->setGeometry(
      x, panel.top() + kTopPadding - NavItem::kRing, m_toggle->sizeHint().width(), m_toggle->sizeHint().height()
  );

  auto place = [&](Entry &entry, int y, bool fromBottom) {
    const int height = entryHeight(entry, itemHeight);
    const int top = fromBottom ? y - height : y;
    entry.slot = QRect(panel.left() + kSidePadding, top, panel.width() - 2 * kSidePadding, height);
    if (entry.item) {
      entry.item->setGeometry(x, top - NavItem::kRing, itemWidth, height + 2 * NavItem::kRing);
    }
    return fromBottom ? top - gap : top + height + gap;
  };

  int top = panel.top() + kTopPadding + NavItem::itemHeight() + 12;
  for (auto &entry : m_entries) {
    if (entry.region != Region::Top || (entry.item && entry.item->isHidden()))
      continue;
    top = place(entry, top, false);
  }
  int bottom = panel.bottom() - kBottomPadding + 1;
  for (int i = int(m_entries.size()) - 1; i >= 0; --i) {
    auto &entry = m_entries[i];
    if (entry.region != Region::Bottom || (entry.item && entry.item->isHidden()))
      continue;
    bottom = place(entry, bottom, true);
  }
}

void NavRail::changeEvent(QEvent *event)
{
  PanelWidget::changeEvent(event);
  // the name of the list is read by a screen reader: it is in the language of the application
  if (event->type() == QEvent::LanguageChange)
    setAccessibleName(tr("Sidebar"));
}

void NavRail::resizeEvent(QResizeEvent *event)
{
  PanelWidget::resizeEvent(event);
  relayout();
}

QSize NavRail::sizeHint() const
{
  const auto room = shadowMargins();
  return QSize(
      widgetWidthFor(expandedPanelWidth()), panelHeightFor(NavItem::itemHeight()) + room.top() + room.bottom()
  );
}

QSize NavRail::minimumSizeHint() const
{
  const auto room = shadowMargins();
  return QSize(widgetWidthFor(collapsedPanelWidth()), panelHeightFor(compactItemHeight()) + room.top() + room.bottom());
}

void NavRail::setSolid(bool solid)
{
  if (m_solid == solid)
    return;
  m_solid = solid;
  update();
}

bool NavRail::isSolid() const
{
  return m_solid;
}

void NavRail::paintEvent(QPaintEvent *event)
{
  if (m_solid) {
    QPainter under(this);
    under.setRenderHint(QPainter::Antialiasing);
    under.setPen(Qt::NoPen);
    under.setBrush(currentTokens().colors.surface);
    under.drawRoundedRect(panelRect(), metrics::radiusCard, metrics::radiusCard);
  }
  PanelWidget::paintEvent(event);

  const auto tokens = currentTokens();
  const auto &c = tokens.colors;
  QPainter painter(this);
  const auto dpr = painter.device()->devicePixelRatioF();
  for (const auto &entry : m_entries) {
    if (entry.item)
      continue;
    if (entry.separator) {
      auto line = c.text;
      line.setAlphaF(0.14);
      const auto y = snapToDevice(entry.slot.center().y(), dpr);
      painter.fillRect(QRectF(entry.slot.left(), y, entry.slot.width(), deviceLineWidth(1, dpr)), line);
      continue;
    }
    // a group title: written in the rail with its labels, and a short line in the rail of icons
    if (m_panelWidth > collapsedPanelWidth() + px(70)) {
      auto font = scaleFont(this->font(), qreal(metrics::fontCaption) / metrics::fontBody);
      font.setWeight(fontWeight(QFont::DemiBold));
      painter.setFont(font);
      painter.setPen(c.textSecondary);
      const QRect box = entry.slot.adjusted(14, 0, 0, 0);
      painter.drawText(
          box, Qt::AlignVCenter | Qt::AlignLeft,
          QFontMetrics(font).elidedText(entry.section, Qt::ElideRight, box.width())
      );
    } else {
      auto line = c.text;
      line.setAlphaF(0.14);
      const auto y = snapToDevice(entry.slot.center().y(), dpr);
      painter.fillRect(QRectF(entry.slot.center().x() - 10, y, 20, deviceLineWidth(1, dpr)), line);
    }
  }
}
