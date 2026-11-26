/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorEditorWidget.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/LookFollower.h"
#include "gui/style/TextSize.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>

using namespace deskflow::server::editor;
namespace editor = deskflow::server::editor;

namespace {

constexpr int kPadding = 12;
constexpr double kHandleSize = 8.0;
constexpr double kGrab = 7.0;
constexpr double kSnapPixels = 8.0;
constexpr double kDragPixels = 3.0;

double luminance(const QColor &color)
{
  return 0.299 * color.redF() + 0.587 * color.greenF() + 0.114 * color.blueF();
}

bool isLeft(Handle h)
{
  return h == Handle::Left || h == Handle::TopLeft || h == Handle::BottomLeft;
}
bool isRight(Handle h)
{
  return h == Handle::Right || h == Handle::TopRight || h == Handle::BottomRight;
}
bool isTop(Handle h)
{
  return h == Handle::Top || h == Handle::TopLeft || h == Handle::TopRight;
}
bool isBottom(Handle h)
{
  return h == Handle::Bottom || h == Handle::BottomLeft || h == Handle::BottomRight;
}

Qt::CursorShape cursorOf(Handle h)
{
  switch (h) {
  case Handle::Left:
  case Handle::Right:
    return Qt::SizeHorCursor;
  case Handle::Top:
  case Handle::Bottom:
    return Qt::SizeVerCursor;
  case Handle::TopLeft:
  case Handle::BottomRight:
    return Qt::SizeFDiagCursor;
  default:
    return Qt::SizeBDiagCursor;
  }
}

} // namespace

MonitorEditorWidget::MonitorEditorWidget(QWidget *parent) : QWidget(parent)
{
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAutoFillBackground(false);
  setAccessibleName(tr("Monitors of the computer"));
  setAccessibleDescription(tr("Drag a monitor to move it. Arrow keys move the selected monitor."));
  installAccessibleInterfaces();
  new deskflow::gui::style::LookFollower(this, [this] { followLook(); });
}

// The colours are the ones of the look that the application has now; with no style layer they are the ones that the
// editor always had
void MonitorEditorWidget::followLook()
{
  m_themed = deskflow::gui::style::isDeskflowStyleInstalled();
  if (m_themed)
    m_theme = deskflow::gui::style::currentCanvasTheme();
  m_backdrop = QPixmap();
  update();
}

const QPixmap &MonitorEditorWidget::backdrop(qreal dpr) const
{
  const auto pixels = QSizeF(size() * dpr).toSize();
  if (m_backdrop.isNull() || m_backdrop.size() != pixels || !qFuzzyCompare(m_backdrop.devicePixelRatio(), dpr))
    m_backdrop = deskflow::gui::style::paintCanvasBackdrop(m_theme, size(), dpr);
  return m_backdrop;
}

QSize MonitorEditorWidget::sizeHint() const
{
  return {640, 300};
}

QSize MonitorEditorWidget::minimumSizeHint() const
{
  return {320, 200};
}

void MonitorEditorWidget::setDrawing(const Layout &layout)
{
  m_layout = layout;
  m_drag = {};
  if (m_selected >= static_cast<int>(m_layout.monitors.size()))
    m_selected = m_layout.monitors.empty() ? -1 : static_cast<int>(m_layout.monitors.size()) - 1;
  m_needsFit = true;
  QWidget::update();
}

void MonitorEditorWidget::setSelected(int index)
{
  if (index >= static_cast<int>(m_layout.monitors.size()))
    index = -1;
  if (index == m_selected)
    return;
  m_selected = index;
  if (index >= 0)
    m_cursor = index;
  Q_EMIT selectionChanged(index);
  QWidget::update();
  if (m_themed && index >= 0)
    deskflow::gui::style::announceSelectedItem(this, index);
}

void MonitorEditorWidget::fit() const
{
  const auto &layout = shown();
  if (layout.monitors.empty()) {
    m_scale = 0.1;
    m_origin = {};
    return;
  }
  const auto box = layout.box();
  // room around the monitors to drag one, and to put one that is added
  const double margin = std::max(box.w, box.h) * 0.22;
  const double width = box.w + 2 * margin;
  const double height = box.h + 2 * margin;
  const double availableW = std::max(1, this->width() - 2 * kPadding);
  const double availableH = std::max(1, this->height() - 2 * kPadding);
  m_scale = std::clamp(std::min(availableW / width, availableH / height), 0.001, 2.0);
  m_origin = {
      this->width() / 2.0 - (box.x + box.w / 2) * m_scale, this->height() / 2.0 - (box.y + box.h / 2) * m_scale
  };
  m_needsFit = false;
}

QPointF MonitorEditorWidget::toWidget(double x, double y) const
{
  if (m_needsFit && !m_drag.active)
    fit();
  return {m_origin.x() + x * m_scale, m_origin.y() + y * m_scale};
}

QPointF MonitorEditorWidget::toUnits(const QPointF &point) const
{
  if (m_needsFit && !m_drag.active)
    fit();
  return {(point.x() - m_origin.x()) / m_scale, (point.y() - m_origin.y()) / m_scale};
}

QRectF MonitorEditorWidget::rectOf(int index) const
{
  const auto &layout = shown();
  if (index < 0 || index >= static_cast<int>(layout.monitors.size()))
    return {};
  const auto r = layout.monitors[static_cast<size_t>(index)].rect();
  return QRectF(toWidget(r.x, r.y), toWidget(r.right(), r.bottom()));
}

QString MonitorEditorWidget::describe(const Monitor &monitor)
{
  return tr("%1 × %2 · %3 %").arg(monitor.pixelsW()).arg(monitor.pixelsH()).arg(std::lround(monitor.scale));
}

QColor MonitorEditorWidget::colorOf(size_t index) const
{
  static const std::array<QColor, 6> colors = {QColor(58, 122, 214), QColor(70, 160, 120), QColor(176, 110, 200),
                                               QColor(222, 146, 64), QColor(60, 168, 190), QColor(200, 90, 110)};
  return colors[index % colors.size()];
}

std::optional<Handle> MonitorEditorWidget::handleAt(const QPointF &point) const
{
  if (m_selected < 0)
    return std::nullopt;
  const auto r = rectOf(m_selected);
  if (r.isEmpty())
    return std::nullopt;
  const bool nearLeft = std::abs(point.x() - r.left()) <= kGrab;
  const bool nearRight = std::abs(point.x() - r.right()) <= kGrab;
  const bool nearTop = std::abs(point.y() - r.top()) <= kGrab;
  const bool nearBottom = std::abs(point.y() - r.bottom()) <= kGrab;
  const bool withinX = point.x() >= r.left() - kGrab && point.x() <= r.right() + kGrab;
  const bool withinY = point.y() >= r.top() - kGrab && point.y() <= r.bottom() + kGrab;
  if (nearTop && nearLeft)
    return Handle::TopLeft;
  if (nearTop && nearRight)
    return Handle::TopRight;
  if (nearBottom && nearLeft)
    return Handle::BottomLeft;
  if (nearBottom && nearRight)
    return Handle::BottomRight;
  if (nearLeft && withinY)
    return Handle::Left;
  if (nearRight && withinY)
    return Handle::Right;
  if (nearTop && withinX)
    return Handle::Top;
  if (nearBottom && withinX)
    return Handle::Bottom;
  return std::nullopt;
}

int MonitorEditorWidget::monitorAt(const QPointF &point) const
{
  for (int i = static_cast<int>(shown().monitors.size()) - 1; i >= 0; --i) {
    if (rectOf(i).contains(point))
      return i;
  }
  return -1;
}

void MonitorEditorWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  m_needsFit = true;
}

void MonitorEditorWidget::paintEvent(QPaintEvent *)
{
  const auto &layout = shown();
  if (m_needsFit && !m_drag.active)
    fit();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto &palette = this->palette();
  if (m_themed) {
    painter.drawPixmap(0, 0, backdrop(painter.device()->devicePixelRatioF()));
  } else {
    painter.fillRect(rect(), palette.color(QPalette::Base));
    painter.setPen(QPen(palette.color(QPalette::Mid), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }

  const auto problems = check(layout);
  std::vector<bool> overlapping(layout.monitors.size(), false);
  std::vector<bool> alone(layout.monitors.size(), false);
  for (const auto &problem : problems) {
    if (problem.kind == Problem::Kind::Overlap) {
      overlapping[problem.first] = true;
      overlapping[problem.second] = true;
    } else if (problem.kind == Problem::Kind::NotConnected) {
      alone[problem.first] = true;
    }
  }

  auto nameFont = font();
  nameFont.setBold(true);
  for (size_t i = 0; i < layout.monitors.size(); ++i) {
    const auto &monitor = layout.monitors[i];
    const auto r = rectOf(static_cast<int>(i));
    // with the style layer every monitor is the same neutral: colour is for the selected one and for what is wrong
    const auto color = m_themed ? m_theme.fill : colorOf(i);
    const bool selected = static_cast<int>(i) == m_selected;
    const auto edge = m_themed ? (selected ? m_theme.selected : m_theme.border)
                               : (selected ? palette.color(QPalette::Highlight).darker(110) : color.darker(150));
    painter.setPen(QPen(edge, selected ? 2.5 : 1.5));
    painter.setBrush(color);
    painter.drawRoundedRect(r.adjusted(1, 1, -1, -1), 3, 3);
    if (i == 0) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(m_themed ? m_theme.mainBar : QColor(255, 255, 255, 200));
      painter.drawRoundedRect(
          QRectF(r.left() + 4, r.top() + 4, std::max(0.0, r.width() - 8), std::min(6.0, r.height() / 8)), 2, 2
      );
    }
    if (overlapping[i]) {
      painter.setPen(QPen(m_themed ? m_theme.error : QColor(210, 30, 40), 2, Qt::DashLine));
      painter.setBrush(QBrush(m_themed ? m_theme.errorHatch : QColor(210, 30, 40, 90), Qt::BDiagPattern));
      painter.drawRect(r.adjusted(2, 2, -2, -2));
    }
    if (alone[i]) {
      painter.setPen(QPen(m_themed ? m_theme.error : QColor(210, 30, 40), 2, Qt::DashLine));
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(r.adjusted(2, 2, -2, -2), 3, 3);
    }

    const auto area = r.adjusted(6, 10, -6, -6);
    if (area.width() > 30 && area.height() > 18) {
      painter.setPen(m_themed ? m_theme.text : luminance(color) > 0.6 ? QColor(24, 24, 24) : QColor(255, 255, 255));
      auto big = nameFont;
      big.setPixelSize(
          std::clamp(static_cast<int>(area.height() * 0.2), deskflow::gui::style::px(11), deskflow::gui::style::px(22))
      );
      painter.setFont(big);
      const QString line = tr("%1 × %2").arg(monitor.pixelsW()).arg(monitor.pixelsH());
      QString second = tr("%1 %").arg(std::lround(monitor.scale));
      if (const auto aspect = aspectNameOf(monitor.w, monitor.h); !aspect.empty())
        second += QStringLiteral(" · ") + QString::fromStdString(aspect);
      if (i == 0)
        second += QStringLiteral(" · ") + tr("main");
      auto small = font();
      small.setPixelSize(std::max(deskflow::gui::style::px(10), big.pixelSize() * 3 / 4));
      const QFontMetricsF bigMetrics(big);
      const QFontMetricsF smallMetrics(small);
      const double total = bigMetrics.height() + smallMetrics.height();
      double y = area.center().y() - total / 2;
      painter.drawText(
          QRectF(area.left(), y, area.width(), bigMetrics.height()), Qt::AlignHCenter | Qt::AlignVCenter,
          bigMetrics.elidedText(line, Qt::ElideRight, static_cast<int>(area.width()))
      );
      y += bigMetrics.height();
      painter.setFont(small);
      if (m_themed)
        painter.setPen(m_theme.textSecondary);
      painter.drawText(
          QRectF(area.left(), y, area.width(), smallMetrics.height()), Qt::AlignHCenter | Qt::AlignVCenter,
          smallMetrics.elidedText(second, Qt::ElideRight, static_cast<int>(area.width()))
      );
    }
  }

  // the handles of the selected monitor
  if (m_selected >= 0 && m_selected < static_cast<int>(layout.monitors.size())) {
    const auto r = rectOf(m_selected);
    const std::array<QPointF, 8> points = {
        r.topLeft(),
        QPointF(r.center().x(), r.top()),
        r.topRight(),
        QPointF(r.left(), r.center().y()),
        QPointF(r.right(), r.center().y()),
        r.bottomLeft(),
        QPointF(r.center().x(), r.bottom()),
        r.bottomRight()
    };
    painter.setPen(QPen(m_themed ? m_theme.selected : palette.color(QPalette::Highlight).darker(130), 1.5));
    painter.setBrush(m_themed ? m_theme.fill : palette.color(QPalette::Base));
    for (const auto &point : points)
      painter.drawRect(QRectF(point.x() - kHandleSize / 2, point.y() - kHandleSize / 2, kHandleSize, kHandleSize));
  }

  // the guides of the edges that are in line or touch
  for (const auto &guide : m_drag.guides) {
    painter.setPen(QPen(
        m_themed ? m_theme.guide : QColor(226, 42, 142), guide.touching ? 2.0 : 1.5,
        guide.touching ? Qt::SolidLine : Qt::DashLine
    ));
    if (guide.vertical)
      painter.drawLine(toWidget(guide.position, guide.from), toWidget(guide.position, guide.to));
    else
      painter.drawLine(toWidget(guide.from, guide.position), toWidget(guide.to, guide.position));
  }

  // where the keys are
  if (m_themed && hasFocus() && !m_readOnly) {
    QList<QRectF> around;
    if (m_selected >= 0 && m_selected < static_cast<int>(layout.monitors.size()))
      around << rectOf(m_selected);
    deskflow::gui::style::paintCanvasFocus(&painter, m_theme, size(), around);
  }

  // the size, while a monitor is dragged
  if (m_drag.active && m_drag.moved && !m_drag.info.isEmpty()) {
    painter.setFont(font());
    const QFontMetricsF metrics(painter.font());
    const QSizeF size(metrics.horizontalAdvance(m_drag.info) + 20, metrics.height() + 10);
    auto at = m_drag.pointer + QPointF(16, 18);
    at.setX(std::clamp(at.x(), 4.0, std::max(4.0, width() - size.width() - 4)));
    at.setY(std::clamp(at.y(), 4.0, std::max(4.0, height() - size.height() - 4)));
    const QRectF bubble(at, size);
    painter.setPen(m_themed ? QPen(m_theme.bubble, 1) : QPen(QColor(0, 0, 0, 90), 1));
    painter.setBrush(m_themed ? m_theme.bubble : QColor(32, 32, 32, 230));
    painter.drawRoundedRect(bubble, 5, 5);
    painter.setPen(m_themed ? m_theme.bubbleText : QColor(255, 255, 255));
    painter.drawText(bubble, Qt::AlignCenter, m_drag.info);
  }
}

void MonitorEditorWidget::setReadOnly(bool readOnly)
{
  m_readOnly = readOnly;
  if (readOnly) {
    m_drag = {};
    unsetCursor();
  }
}

void MonitorEditorWidget::mousePressEvent(QMouseEvent *event)
{
  if (m_readOnly) {
    event->ignore();
    return;
  }
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  setFocus();
  const auto point = event->position();
  const auto units = toUnits(point);
  m_drag = {};
  if (const auto handle = handleAt(point)) {
    const auto &m = m_layout.monitors[static_cast<size_t>(m_selected)];
    m_drag.active = true;
    m_drag.resizing = true;
    m_drag.handle = *handle;
    m_drag.index = m_selected;
    m_drag.grabX = units.x() - (isLeft(*handle) ? m.x : m.right());
    m_drag.grabY = units.y() - (isTop(*handle) ? m.y : m.bottom());
  } else if (const int index = monitorAt(point); index >= 0) {
    setSelected(index);
    const auto &m = m_layout.monitors[static_cast<size_t>(index)];
    m_drag.active = true;
    m_drag.index = index;
    m_drag.grabX = units.x() - m.x;
    m_drag.grabY = units.y() - m.y;
  } else {
    setSelected(-1);
    return;
  }
  m_drag.pressed = point;
  m_drag.pointer = point;
  m_drag.start = m_layout;
  m_drag.preview = m_layout;
  QWidget::update();
}

void MonitorEditorWidget::dragTo(const QPointF &point)
{
  m_drag.pointer = point;
  if (!m_drag.moved && QLineF(point, m_drag.pressed).length() < kDragPixels)
    return;
  m_drag.moved = true;
  const auto units = toUnits(point);
  const double radius = kSnapPixels / m_scale;
  const auto index = static_cast<size_t>(m_drag.index);
  if (m_drag.resizing) {
    Constraints constraints;
    if (m_targetSize && m_drag.start.monitors.size() == 1)
      constraints.targetSize = m_targetSize;
    const auto result = editor::resize(
        m_drag.start, index, m_drag.handle, units.x() - m_drag.grabX, units.y() - m_drag.grabY, radius, constraints
    );
    if (!result.refused) {
      m_drag.preview = result.layout;
      m_drag.guides = result.guides;
      QString info = describe(result.layout.monitors[index]);
      if (!result.aspect.empty())
        info += QStringLiteral(" · ") + QString::fromStdString(result.aspect);
      if (result.preset)
        info += QStringLiteral(" · ") + tr("standard size");
      if (result.target)
        info += QStringLiteral(" · ") + tr("= the screen it reports");
      m_drag.info = info;
    }
  } else {
    const auto result = editor::move(m_drag.start, index, units.x() - m_drag.grabX, units.y() - m_drag.grabY, radius);
    if (!result.refused) {
      m_drag.preview = result.layout;
      m_drag.guides = result.guides;
      m_drag.info = describe(result.layout.monitors[index]);
    }
  }
  QWidget::update();
}

void MonitorEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (m_readOnly) {
    event->ignore();
    return;
  }
  if (m_drag.active) {
    dragTo(event->position());
    return;
  }
  if (const auto handle = handleAt(event->position()))
    setCursor(cursorOf(*handle));
  else
    unsetCursor();
}

void MonitorEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || !m_drag.active) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  const auto drag = m_drag;
  m_drag = {};
  if (drag.moved && !(drag.preview == m_layout)) {
    m_layout = drag.preview;
    m_needsFit = true;
    Q_EMIT drawingChanged(m_layout);
  }
  QWidget::update();
}

void MonitorEditorWidget::leaveEvent(QEvent *event)
{
  unsetCursor();
  QWidget::leaveEvent(event);
}

void MonitorEditorWidget::commit(const Layout &layout)
{
  if (layout == m_layout)
    return;
  m_layout = layout;
  m_needsFit = true;
  // a monitor that is gone cannot stay selected
  const auto count = static_cast<int>(m_layout.monitors.size());
  const auto selected = m_selected >= count ? count - 1 : m_selected;
  if (selected != m_selected) {
    m_selected = selected;
    Q_EMIT selectionChanged(m_selected);
  }
  Q_EMIT drawingChanged(m_layout);
  QWidget::update();
}

void MonitorEditorWidget::keyPressEvent(QKeyEvent *event)
{
  if (m_readOnly) {
    event->ignore();
    return;
  }
  if (m_themed && m_selected < 0 && !m_drag.active &&
      (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
      selectAtTheKeys()) {
    event->accept();
    return;
  }
  if (m_selected < 0 || m_selected >= static_cast<int>(m_layout.monitors.size()) || m_drag.active) {
    QWidget::keyPressEvent(event);
    return;
  }
  const auto index = static_cast<size_t>(m_selected);
  const double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
  const auto &m = m_layout.monitors[index];
  switch (event->key()) {
  case Qt::Key_Left:
    commit(editor::move(m_layout, index, m.x - step, m.y, 0).layout);
    break;
  case Qt::Key_Right:
    commit(editor::move(m_layout, index, m.x + step, m.y, 0).layout);
    break;
  case Qt::Key_Up:
    commit(editor::move(m_layout, index, m.x, m.y - step, 0).layout);
    break;
  case Qt::Key_Down:
    commit(editor::move(m_layout, index, m.x, m.y + step, 0).layout);
    break;
  case Qt::Key_R:
    commit(rotated(m_layout, index));
    break;
  case Qt::Key_Delete:
  case Qt::Key_Backspace:
    commit(removed(m_layout, index));
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }
  event->accept();
}

bool MonitorEditorWidget::selectAtTheKeys()
{
  const auto count = static_cast<int>(m_layout.monitors.size());
  if (count == 0)
    return false;
  setSelected(m_cursor >= 0 && m_cursor < count ? m_cursor : 0);
  return true;
}

// Tab goes from one monitor to the next, and out of the editor after the last (and Shift+Tab before the first)
bool MonitorEditorWidget::focusNextPrevChild(bool next)
{
  if (m_themed && hasFocus() && !m_readOnly) {
    const auto target = m_selected + (next ? 1 : -1);
    if (target >= 0 && target < static_cast<int>(m_layout.monitors.size())) {
      setSelected(target);
      return true;
    }
  }
  return QWidget::focusNextPrevChild(next);
}

void MonitorEditorWidget::focusInEvent(QFocusEvent *event)
{
  QWidget::focusInEvent(event);
  const auto reason = event->reason();
  const auto count = static_cast<int>(m_layout.monitors.size());
  if (m_themed && !m_readOnly && m_selected < 0 && count > 0 &&
      (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason))
    setSelected(reason == Qt::TabFocusReason ? 0 : count - 1);
  update();
}

void MonitorEditorWidget::focusOutEvent(QFocusEvent *event)
{
  QWidget::focusOutEvent(event);
  update();
}

//
// What a screen reader is told
//

int MonitorEditorWidget::accessibleItemCount() const
{
  return static_cast<int>(m_layout.monitors.size());
}

QString MonitorEditorWidget::accessibleItemName(int index) const
{
  return index >= 0 && index < accessibleItemCount() ? tr("Monitor %1").arg(index + 1) : QString();
}

QString MonitorEditorWidget::accessibleItemDescription(int index) const
{
  if (index < 0 || index >= accessibleItemCount())
    return {};
  QStringList parts{describe(m_layout.monitors[static_cast<size_t>(index)])};
  if (index == 0)
    parts << tr("main");
  // what is wrong is said in words: it is drawn, and a colour is not heard
  for (const auto &problem : check(m_layout)) {
    const auto i = static_cast<size_t>(index);
    if (problem.kind == Problem::Kind::Overlap && (problem.first == i || problem.second == i))
      parts << tr("overlaps another monitor");
    else if (problem.kind == Problem::Kind::NotConnected && problem.first == i)
      parts << tr("does not touch the others");
  }
  return parts.join(QStringLiteral(", "));
}

QRect MonitorEditorWidget::accessibleItemRect(int index) const
{
  return rectOf(index).toAlignedRect();
}

bool MonitorEditorWidget::accessibleItemSelected(int index) const
{
  return index >= 0 && index == m_selected;
}

void MonitorEditorWidget::selectAccessibleItem(int index)
{
  if (m_readOnly || index < 0 || index >= accessibleItemCount())
    return;
  setFocus(Qt::OtherFocusReason);
  setSelected(index);
}
