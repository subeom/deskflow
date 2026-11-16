/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementCanvas.h"

#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace deskflow::server::arrangement;

namespace {

// how near, on the screen, an edge has to be to snap
constexpr double kSnapPixels = 12.0;
// how far, on the screen, the pointer goes before a press is a drag
constexpr double kDragPixels = 3.0;
constexpr double kGrabPixels = 7.0;
constexpr double kHandlePixels = 8.0;

namespace editor = deskflow::server::editor;

bool isLeftHandle(editor::Handle h)
{
  return h == editor::Handle::Left || h == editor::Handle::TopLeft || h == editor::Handle::BottomLeft;
}

bool isTopHandle(editor::Handle h)
{
  return h == editor::Handle::Top || h == editor::Handle::TopLeft || h == editor::Handle::TopRight;
}

Qt::CursorShape cursorOf(editor::Handle h)
{
  using enum editor::Handle;
  switch (h) {
  case Left:
  case Right:
    return Qt::SizeHorCursor;
  case Top:
  case Bottom:
    return Qt::SizeVerCursor;
  case TopLeft:
  case BottomRight:
    return Qt::SizeFDiagCursor;
  default:
    return Qt::SizeBDiagCursor;
  }
}
constexpr int kPadding = 10;

double luminance(const QColor &color)
{
  return 0.299 * color.redF() + 0.587 * color.greenF() + 0.114 * color.blueF();
}

QString sizeText(const Computer &computer)
{
  const auto shape = computer.shape();
  QString text = QStringLiteral("%1 × %2").arg(shape.w).arg(shape.h);
  const auto count = computer.distinctMonitors().size();
  if (count > 1)
    text = QStringLiteral("%1 · %2").arg(ArrangementCanvas::tr("%1 displays").arg(count), text);
  return text;
}

// a computer that is shown smaller than its pixels says by how much: a Windows scale of 125 % is a zoom of 0.8
QString zoomText(const Computer &computer)
{
  if (std::abs(computer.zoom - 1.0) <= 0.005)
    return {};
  return ArrangementCanvas::tr("scale %1 %").arg(std::lround(100.0 / computer.zoom));
}

} // namespace

ArrangementCanvas::ArrangementCanvas(QWidget *parent) : QWidget(parent)
{
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAutoFillBackground(false);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAccessibleName(tr("Arrangement of the computers"));
  setAccessibleDescription(tr("Drag a computer to put it where its screen is. Arrow keys move the selected computer."));
}

QSize ArrangementCanvas::sizeHint() const
{
  // it takes whatever room there is; what it asks for must not make the tab that has it taller than the others are
  return {640, 260};
}

QSize ArrangementCanvas::minimumSizeHint() const
{
  return {360, 220};
}

void ArrangementCanvas::setArrangement(const Arrangement &arrangement)
{
  cancelDrag();
  m_arrangement = arrangement;
  if (indexOf(m_selected) < 0)
    m_selected.clear();
  recompute();
  m_needsFit = !m_manualView;
  update();
}

void ArrangementCanvas::recompute()
{
  m_derived = deriveLinks(m_arrangement);
  m_issues = validate(m_arrangement);
  m_issues.insert(m_issues.end(), m_derived.issues.begin(), m_derived.issues.end());
}

void ArrangementCanvas::setStatus(const QString &computer, Status status)
{
  m_status.insert(computer, status);
  update();
}

void ArrangementCanvas::setSelected(const QString &computer)
{
  const auto name = indexOf(computer) < 0 ? QString() : computer;
  if (name == m_selected)
    return;
  m_selected = name;
  Q_EMIT selectedChanged(m_selected);
  update();
}

void ArrangementCanvas::zoomToFit()
{
  m_manualView = false;
  m_needsFit = true;
  update();
}

int ArrangementCanvas::indexOf(const QString &computer) const
{
  return computer.isEmpty() ? -1 : m_arrangement.indexOf(computer.toStdString());
}

Arrangement ArrangementCanvas::shownArrangement() const
{
  auto shown = m_arrangement;
  if (m_drag.resizing) {
    if (m_drag.previewValid)
      shown.computers[static_cast<size_t>(m_drag.index)] = m_drag.preview;
  } else if (m_drag.index >= 0 && m_drag.moved) {
    shown.computers[static_cast<size_t>(m_drag.index)].originX = m_drag.snapped.originX;
    shown.computers[static_cast<size_t>(m_drag.index)].originY = m_drag.snapped.originY;
  }
  return shown;
}

QRectF ArrangementCanvas::arrangementBounds(const Arrangement &arrangement) const
{
  QRectF bounds;
  for (const auto &computer : arrangement.computers) {
    for (const auto &r : computer.monitorRects())
      bounds = bounds.united(QRectF(r.x, r.y, r.w, r.h));
  }
  return bounds;
}

void ArrangementCanvas::ensureFit() const
{
  if (m_needsFit && !m_manualView)
    fit();
  m_needsFit = false;
}

void ArrangementCanvas::fit() const
{
  auto bounds = arrangementBounds(m_arrangement);
  if (bounds.isEmpty()) {
    m_scale = 0.1;
    m_offset = QPointF(width() / 2.0, height() / 2.0);
    return;
  }
  // room around, so that a computer can be put on any side of the others
  const auto margin = 0.12 * std::max(bounds.width(), bounds.height());
  bounds.adjust(-margin, -margin, margin, margin);
  const QRectF available = QRectF(rect()).adjusted(kPadding, kPadding, -kPadding, -kPadding);
  m_scale = std::clamp(std::min(available.width() / bounds.width(), available.height() / bounds.height()), 0.002, 2.0);
  m_offset = available.center() - bounds.center() * m_scale;
}

QPointF ArrangementCanvas::toWidget(double x, double y) const
{
  ensureFit();
  return {m_offset.x() + x * m_scale, m_offset.y() + y * m_scale};
}

QRectF ArrangementCanvas::toWidget(const Rect &rect) const
{
  const auto topLeft = toWidget(rect.x, rect.y);
  return {topLeft, QSizeF(rect.w * m_scale, rect.h * m_scale)};
}

QRectF ArrangementCanvas::boxOf(const QString &computer) const
{
  const auto index = indexOf(computer);
  if (index < 0)
    return {};
  return toWidget(m_arrangement.computers[static_cast<size_t>(index)].box());
}

QString ArrangementCanvas::computerAt(const QPointF &point) const
{
  ensureFit();
  const auto shown = shownArrangement();
  // the one on top, which is the one that was dragged last, is the last in the order of painting
  for (auto i = static_cast<int>(shown.computers.size()) - 1; i >= 0; --i) {
    const auto &computer = shown.computers[static_cast<size_t>(i)];
    for (const auto &r : computer.monitorRects()) {
      if (toWidget(r).contains(point))
        return QString::fromStdString(computer.name);
    }
  }
  return {};
}

QColor ArrangementCanvas::colorOf(size_t index, bool server) const
{
  if (server)
    return QColor::fromHslF(0.60, 0.78, 0.55);
  // hues apart from each other and from the blue of the server
  static constexpr std::array<double, 7> hues = {0.07, 0.42, 0.78, 0.25, 0.93, 0.13, 0.50};
  return QColor::fromHslF(hues.at(index % hues.size()), 0.52, 0.62);
}

//
// Painting
//

void ArrangementCanvas::paintEvent(QPaintEvent *)
{
  ensureFit();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  const auto base = palette().color(QPalette::Base);
  const auto frame = palette().color(QPalette::Mid);
  painter.fillRect(rect(), base);
  painter.setPen(QPen(frame, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

  const auto shown = shownArrangement();
  const bool dragging = m_drag.index >= 0 && m_drag.moved && !m_drag.resizing;
  const bool resizing = m_drag.resizing && m_drag.previewValid;
  const auto derived = dragging || resizing ? deriveLinks(shown) : m_derived;
  auto issues = m_issues;
  if (dragging || resizing) {
    issues = validate(shown);
    issues.insert(issues.end(), derived.issues.begin(), derived.issues.end());
  }

  std::vector<bool> overlapping(shown.computers.size(), false);
  std::vector<bool> unconnected(shown.computers.size(), false);
  for (const auto &issue : issues) {
    for (size_t i = 0; i < shown.computers.size(); ++i) {
      const auto &name = shown.computers[i].name;
      if (issue.kind == Issue::Kind::Overlap && (issue.computer == name || issue.other == name))
        overlapping[i] = true;
      if (issue.kind == Issue::Kind::NotConnected && issue.computer == name)
        unconnected[i] = true;
    }
  }

  if (shown.computers.empty()) {
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, tr("No computers yet. Add a computer to arrange it."));
    return;
  }

  // the dragged computer is on top of the others, and the selected one next
  std::vector<size_t> order;
  for (size_t i = 0; i < shown.computers.size(); ++i) {
    if (static_cast<int>(i) != m_drag.index && QString::fromStdString(shown.computers[i].name) != m_selected)
      order.push_back(i);
  }
  for (size_t i = 0; i < shown.computers.size(); ++i) {
    if (static_cast<int>(i) != m_drag.index && QString::fromStdString(shown.computers[i].name) == m_selected)
      order.push_back(i);
  }
  if (m_drag.index >= 0)
    order.push_back(static_cast<size_t>(m_drag.index));
  for (const auto i : order)
    paintComputer(painter, shown, i, overlapping[i], unconnected[i], static_cast<int>(i) == m_drag.index && dragging);

  // where the computers touch: this is where the pointer goes from one to the other
  for (const auto &contact : derived.contacts) {
    const bool vertical = contact.sideA == Direction::Left || contact.sideA == Direction::Right;
    const auto a = vertical ? toWidget(contact.line, contact.from) : toWidget(contact.from, contact.line);
    const auto b = vertical ? toWidget(contact.line, contact.to) : toWidget(contact.to, contact.line);
    QPen outline(palette().color(QPalette::Base), 7, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(outline);
    painter.drawLine(a, b);
    QPen link(QColor(24, 168, 92), 4, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(link);
    painter.drawLine(a, b);
  }

  // the outline of the computer that the pointer is at, and the handles of that one and of the selected one
  const auto drawHandles = [&](int index) {
    if (resizeKindOf(index) == ResizeKind::None)
      return;
    const auto r = toWidget(shown.computers[static_cast<size_t>(index)].box());
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
    painter.setPen(QPen(palette().color(QPalette::Highlight).darker(130), 1.5));
    painter.setBrush(palette().color(QPalette::Base));
    for (const auto &point : points) {
      painter.drawRect(
          QRectF(point.x() - kHandlePixels / 2, point.y() - kHandlePixels / 2, kHandlePixels, kHandlePixels)
      );
    }
  };
  if (const auto hovered = indexOf(m_hovered); hovered >= 0 && !m_drag.resizing && !dragging) {
    const auto r = toWidget(shown.computers[static_cast<size_t>(hovered)].box());
    painter.setPen(QPen(palette().color(QPalette::Highlight), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    drawHandles(hovered);
  }
  if (const auto selected = indexOf(m_selected); selected >= 0 && selected != indexOf(m_hovered))
    drawHandles(selected);

  // the guides of the edges that are in line, as macOS draws them while a display is dragged
  for (const auto &guide : resizing ? m_drag.resizeGuides : std::vector<deskflow::server::arrangement::Guide>()) {
    painter.setPen(QPen(QColor(226, 42, 142), 1.5, Qt::DashLine));
    if (guide.vertical)
      painter.drawLine(toWidget(guide.position, guide.from), toWidget(guide.position, guide.to));
    else
      painter.drawLine(toWidget(guide.from, guide.position), toWidget(guide.to, guide.position));
  }
  if (dragging) {
    for (const auto &guide : m_drag.snapped.guides) {
      QPen pen(QColor(226, 42, 142), guide.touching ? 2.0 : 1.0, guide.touching ? Qt::SolidLine : Qt::DashLine);
      painter.setPen(pen);
      if (guide.vertical)
        painter.drawLine(toWidget(guide.position, guide.from), toWidget(guide.position, guide.to));
      else
        painter.drawLine(toWidget(guide.from, guide.position), toWidget(guide.to, guide.position));
    }
  }

  // the size, while the outline of a computer is dragged
  if (m_drag.resizing && m_drag.moved && m_drag.previewValid && !m_drag.info.isEmpty()) {
    const QFontMetricsF metrics(painter.font());
    const QSizeF size(metrics.horizontalAdvance(m_drag.info) + 20, metrics.height() + 10);
    auto at = m_drag.pointer + QPointF(16, 18);
    at.setX(std::clamp(at.x(), 4.0, std::max(4.0, width() - size.width() - 4)));
    at.setY(std::clamp(at.y(), 4.0, std::max(4.0, height() - size.height() - 4)));
    const QRectF bubble(at, size);
    painter.setPen(QPen(QColor(0, 0, 0, 90), 1));
    painter.setBrush(QColor(32, 32, 32, 230));
    painter.drawRoundedRect(bubble, 5, 5);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(bubble, Qt::AlignCenter, m_drag.info);
  }
}

void ArrangementCanvas::setReportedSizes(const QHash<QString, QSize> &sizes)
{
  m_reportedSizes.clear();
  for (auto it = sizes.cbegin(); it != sizes.cend(); ++it)
    m_reportedSizes.insert(it.key().toLower(), it.value());
}

ArrangementCanvas::ResizeKind ArrangementCanvas::resizeKindOf(int index) const
{
  if (index < 0 || index >= static_cast<int>(m_arrangement.computers.size()))
    return ResizeKind::None;
  const auto &computer = m_arrangement.computers[static_cast<size_t>(index)];
  if (computer.isServer || !(computer.zoom > 0) || computer.monitorRects().empty())
    return ResizeKind::None;
  if (computer.distinctMonitors().size() > 1)
    return ResizeKind::Scale;
  const auto reported = m_reportedSizes.value(QString::fromStdString(computer.name).toLower());
  return reported.width() > 0 && reported.height() > 0 ? ResizeKind::Scale : ResizeKind::Free;
}

QPointF ArrangementCanvas::toUnits(const QPointF &point) const
{
  ensureFit();
  return {(point.x() - m_offset.x()) / m_scale, (point.y() - m_offset.y()) / m_scale};
}

std::optional<ArrangementCanvas::Hit> ArrangementCanvas::outlineAt(const QPointF &point) const
{
  using enum editor::Handle;
  std::optional<Hit> best;
  double bestScore = std::numeric_limits<double>::max();
  for (size_t i = 0; i < m_arrangement.computers.size(); ++i) {
    const auto &computer = m_arrangement.computers[i];
    if (resizeKindOf(static_cast<int>(i)) == ResizeKind::None)
      continue;
    const auto r = toWidget(computer.box());
    // a small computer keeps a middle that it is dragged by
    const double tolerance = std::min(kGrabPixels, std::min(r.width(), r.height()) / 3.0);
    const double dl = std::abs(point.x() - r.left());
    const double dr = std::abs(point.x() - r.right());
    const double dt = std::abs(point.y() - r.top());
    const double db = std::abs(point.y() - r.bottom());
    const bool withinX = point.x() >= r.left() - tolerance && point.x() <= r.right() + tolerance;
    const bool withinY = point.y() >= r.top() - tolerance && point.y() <= r.bottom() + tolerance;
    std::optional<editor::Handle> handle;
    double distance = 0;
    if (dt <= tolerance && dl <= tolerance) {
      handle = TopLeft;
      distance = std::max(dt, dl);
    } else if (dt <= tolerance && dr <= tolerance) {
      handle = TopRight;
      distance = std::max(dt, dr);
    } else if (db <= tolerance && dl <= tolerance) {
      handle = BottomLeft;
      distance = std::max(db, dl);
    } else if (db <= tolerance && dr <= tolerance) {
      handle = BottomRight;
      distance = std::max(db, dr);
    } else if (dl <= tolerance && withinY) {
      handle = Left;
      distance = dl;
    } else if (dr <= tolerance && withinY) {
      handle = Right;
      distance = dr;
    } else if (dt <= tolerance && withinX) {
      handle = Top;
      distance = dt;
    } else if (db <= tolerance && withinX) {
      handle = Bottom;
      distance = db;
    }
    if (!handle)
      continue;
    // the nearer outline, then the computer that the pointer is in, then the selected one
    const double score =
        distance + (r.contains(point) ? 0.0 : 0.3) + (QString::fromStdString(computer.name) == m_selected ? 0.0 : 0.1);
    if (score < bestScore) {
      bestScore = score;
      best = Hit{static_cast<int>(i), *handle};
    }
  }
  return best;
}

void ArrangementCanvas::setAssumedSizes(const QStringList &computers)
{
  m_assumed = computers;
}

void ArrangementCanvas::paintComputer(
    QPainter &painter, const Arrangement &shown, size_t index, bool overlapping, bool unconnected, bool dragged
) const
{
  const auto &computer = shown.computers[index];
  const auto name = QString::fromStdString(computer.name);
  const auto rects = computer.monitorRects();
  if (rects.empty())
    return;

  const auto color = colorOf(index, computer.isServer);
  const auto border = color.darker(150);
  // its size is a guess: nothing says what it is
  const bool assumed = m_assumed.contains(name, Qt::CaseInsensitive);
  const auto text = luminance(color) > 0.6 ? QColor(24, 24, 24) : QColor(255, 255, 255);

  painter.save();
  if (dragged)
    painter.setOpacity(0.85);

  // the monitors: the one that has the origin of the computer has the bar of the main display, like in macOS
  const auto monitors = computer.distinctMonitors();
  for (size_t i = 0; i < rects.size(); ++i) {
    const auto r = toWidget(rects[i]);
    if (dragged) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(0, 0, 0, 40));
      painter.drawRoundedRect(r.translated(3, 4), 4, 4);
    }
    painter.setPen(QPen(border, 1.5, assumed ? Qt::DashLine : Qt::SolidLine));
    painter.setBrush(color);
    painter.drawRoundedRect(r.adjusted(0.75, 0.75, -0.75, -0.75), 3, 3);
    if (monitors[i].contains(0, 0)) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(255, 255, 255, 200));
      painter.drawRoundedRect(
          QRectF(r.left() + 3, r.top() + 3, std::max(0.0, r.width() - 6), std::min(5.0, r.height() / 8)), 1.5, 1.5
      );
    }
    if (overlapping) {
      painter.setPen(QPen(QColor(210, 30, 40), 2, Qt::DashLine));
      painter.setBrush(QBrush(QColor(210, 30, 40, 90), Qt::BDiagPattern));
      painter.drawRect(r.adjusted(1, 1, -1, -1));
    }
  }

  // the label goes on the biggest monitor
  size_t biggest = 0;
  for (size_t i = 1; i < rects.size(); ++i) {
    if (rects[i].w * rects[i].h > rects[biggest].w * rects[biggest].h)
      biggest = i;
  }
  const auto area = toWidget(rects[biggest]).adjusted(6, 6, -6, -6);
  if (area.width() > 24 && area.height() > 16) {
    auto nameFont = font();
    nameFont.setBold(true);
    const auto basePixels = font().pixelSize() > 0 ? font().pixelSize() : 12;
    auto pixels = static_cast<int>(std::clamp(area.height() * 0.16, static_cast<double>(basePixels), 22.0));
    // as big as the name fits on the monitor, down to a size that can still be read
    nameFont.setPixelSize(pixels);
    while (pixels > 10 && QFontMetricsF(nameFont).horizontalAdvance(name) > area.width()) {
      --pixels;
      nameFont.setPixelSize(pixels);
    }
    auto smallFont = font();
    smallFont.setPixelSize(std::max(9, static_cast<int>(nameFont.pixelSize() * 0.78)));
    const QFontMetricsF nameMetrics(nameFont);
    const QFontMetricsF smallMetrics(smallFont);

    QStringList lines;
    if (computer.isServer)
      lines << tr("this computer");
    lines << sizeText(computer);
    if (assumed)
      lines << tr("size assumed");
    if (const auto zoom = zoomText(computer); !zoom.isEmpty())
      lines << zoom;

    const auto total = nameMetrics.height() + smallMetrics.height() * static_cast<double>(lines.size());
    auto y = area.center().y() - total / 2;
    painter.setPen(text);
    painter.setFont(nameFont);
    painter.drawText(
        QRectF(area.left(), y, area.width(), nameMetrics.height()), Qt::AlignHCenter | Qt::AlignVCenter,
        nameMetrics.elidedText(name, Qt::ElideMiddle, static_cast<int>(area.width()))
    );
    y += nameMetrics.height();
    painter.setFont(smallFont);
    painter.setPen(luminance(color) > 0.6 ? QColor(24, 24, 24, 190) : QColor(255, 255, 255, 215));
    for (const auto &line : lines) {
      painter.drawText(
          QRectF(area.left(), y, area.width(), smallMetrics.height()), Qt::AlignHCenter | Qt::AlignVCenter,
          smallMetrics.elidedText(line, Qt::ElideRight, static_cast<int>(area.width()))
      );
      y += smallMetrics.height();
    }
  }

  // whether it is connected, and what nothing touches
  const auto status = m_status.value(name, Status::Unknown);
  const auto box = toWidget(computer.box());
  if (status != Status::Unknown) {
    painter.setPen(QPen(border, 1));
    painter.setBrush(status == Status::Connected ? QColor(40, 190, 90) : QColor(150, 150, 150));
    painter.drawEllipse(QPointF(box.right() - 9, box.top() + 9), 4.5, 4.5);
  }
  if (unconnected) {
    painter.setPen(QPen(QColor(235, 150, 0), 2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    for (const auto &r : rects)
      painter.drawRect(toWidget(r).adjusted(2, 2, -2, -2));
  }
  if (name == m_selected) {
    painter.setPen(QPen(palette().color(QPalette::Highlight), 3));
    painter.setBrush(Qt::NoBrush);
    for (const auto &r : rects)
      painter.drawRoundedRect(toWidget(r).adjusted(-1, -1, 1, 1), 4, 4);
  }
  painter.restore();
}

//
// The mouse and the keys
//

void ArrangementCanvas::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  setFocus();
  const auto name = computerAt(event->position());
  m_drag = {};
  m_hovered.clear();
  const auto hit = event->button() == Qt::LeftButton ? outlineAt(event->position()) : std::nullopt;
  if (hit) {
    // the outline of a computer is dragged to resize it, without the computer being selected first
    const auto &computer = m_arrangement.computers[static_cast<size_t>(hit->index)];
    setSelected(QString::fromStdString(computer.name));
    const auto box = computer.box();
    const auto units = toUnits(event->position());
    m_drag.index = hit->index;
    m_drag.resizing = true;
    m_drag.scaleMode = resizeKindOf(hit->index) == ResizeKind::Scale;
    m_drag.handle = hit->handle;
    m_drag.button = event->button();
    m_drag.pressed = event->position();
    m_drag.pointer = event->position();
    m_drag.grabX = units.x() - (isLeftHandle(hit->handle) ? box.x : box.right());
    m_drag.grabY = units.y() - (isTopHandle(hit->handle) ? box.y : box.bottom());
    m_drag.shape = computer.shape();
    m_drag.ratio = box.h > 0 ? box.w / box.h : 1.0;
    editor::Monitor monitor;
    monitor.x = box.x;
    monitor.y = box.y;
    monitor.w = box.w;
    monitor.h = box.h;
    monitor.scale = 100.0 / computer.zoom;
    m_drag.startLayout = editor::Layout{{monitor}};
  } else if (!name.isEmpty() && event->button() == Qt::LeftButton) {
    setSelected(name);
    const auto index = indexOf(name);
    const auto &computer = m_arrangement.computers[static_cast<size_t>(index)];
    m_drag.index = index;
    m_drag.button = event->button();
    m_drag.pressed = event->position();
    m_drag.originX = computer.originX;
    m_drag.originY = computer.originY;
    m_drag.snapped.originX = computer.originX;
    m_drag.snapped.originY = computer.originY;
  } else {
    // the background: nothing is selected, and a drag moves the view
    if (event->button() == Qt::LeftButton)
      setSelected(QString());
    m_drag.panning = true;
    m_drag.button = event->button();
    m_drag.pressed = event->position();
    m_drag.offsetAtPress = m_offset;
  }
  event->accept();
}

void ArrangementCanvas::mouseMoveEvent(QMouseEvent *event)
{
  if (m_drag.panning) {
    m_offset = m_drag.offsetAtPress + (event->position() - m_drag.pressed);
    m_manualView = true;
    update();
    return;
  }
  if (m_drag.index >= 0 && m_drag.resizing) {
    m_drag.pointer = event->position();
    const auto delta = event->position() - m_drag.pressed;
    if (!m_drag.moved && std::hypot(delta.x(), delta.y()) < kDragPixels)
      return;
    m_drag.moved = true;
    setCursor(cursorOf(m_drag.handle));
    const auto index = static_cast<size_t>(m_drag.index);
    const auto &computer = m_arrangement.computers[index];
    // Shift keeps the shape, Alt takes the snapping away
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool alt = event->modifiers().testFlag(Qt::AltModifier);
    editor::Constraints constraints;
    constraints.snapAspect = !alt;
    constraints.snapPresets = !alt;
    if (m_drag.scaleMode || shift)
      constraints.lockAspect = m_drag.ratio;
    for (size_t i = 0; i < m_arrangement.computers.size(); ++i) {
      if (i == index)
        continue;
      for (const auto &r : m_arrangement.computers[i].monitorRects())
        constraints.obstacles.push_back(r);
    }
    const auto units = toUnits(event->position());
    const auto result = editor::resize(
        m_drag.startLayout, 0, m_drag.handle, units.x() - m_drag.grabX, units.y() - m_drag.grabY,
        alt ? 0.0 : kSnapPixels / m_scale, constraints
    );
    if (result.refused) {
      update();
      return;
    }
    const auto &m = result.layout.monitors.front();
    const auto &start = m_drag.startLayout.monitors.front();
    auto preview = computer;
    if (m_drag.scaleMode) {
      // the whole computer is scaled: the monitors and the pixels are what they were, the zoom is what changes
      if (m_drag.shape.w <= 0 || m_drag.shape.h <= 0) {
        update();
        return;
      }
      double zoom = m.w / m_drag.shape.w;
      double x = m.x;
      double y = m.y;
      if (!alt && result.guides.empty()) {
        // near a display scale that there is, it is that one
        for (const auto percent : editor::commonScales()) {
          const double wanted = 100.0 / percent;
          if (std::abs(zoom - wanted) / wanted > 0.015)
            continue;
          const double w2 = wanted * m_drag.shape.w;
          const double h2 = wanted * m_drag.shape.h;
          const double x2 = isLeftHandle(m_drag.handle) ? start.right() - w2 : start.x;
          const double y2 = isTopHandle(m_drag.handle) ? start.bottom() - h2 : start.y;
          const bool over =
              std::ranges::any_of(constraints.obstacles, [&](const deskflow::server::arrangement::Rect &o) {
                return std::min(x2 + w2, o.right()) - std::max(x2, o.x) >
                           deskflow::server::arrangement::kTouchTolerance &&
                       std::min(y2 + h2, o.bottom()) - std::max(y2, o.y) >
                           deskflow::server::arrangement::kTouchTolerance;
              });
          if (!over) {
            zoom = wanted;
            x = x2;
            y = y2;
          }
          break;
        }
      }
      if (!(zoom >= deskflow::server::arrangement::kMinZoom && zoom <= deskflow::server::arrangement::kMaxZoom)) {
        update();
        return;
      }
      preview.zoom = zoom;
      preview.originX = x - zoom * m_drag.shape.x;
      preview.originY = y - zoom * m_drag.shape.y;
      m_drag.info =
          tr("%1 × %2 · scale %3 %")
              .arg(m_drag.shape.w)
              .arg(m_drag.shape.h)
              .arg(
                  QString::number(100.0 / zoom, 'f', std::abs(100.0 / zoom - std::round(100.0 / zoom)) < 0.05 ? 0 : 1)
              );
    } else {
      const auto monitor = computer.distinctMonitors().front();
      const auto w = std::max(1, static_cast<int>(std::lround(m.w / computer.zoom)));
      const auto h = std::max(1, static_cast<int>(std::lround(m.h / computer.zoom)));
      preview.monitors = {{monitor.x, monitor.y, w, h}};
      // a size that a layout cannot have (its numbers are 16 bits) is not one that a computer can be given
      if (!deskflow::server::MonitorLayout::create(preview.monitors)) {
        update();
        return;
      }
      // the edge that is not dragged stays where it is: the size is a whole number of the units of the computer
      preview.originX = isLeftHandle(m_drag.handle) ? start.right() - computer.zoom * (monitor.x + w)
                                                    : m.x - computer.zoom * monitor.x;
      preview.originY = isTopHandle(m_drag.handle) ? start.bottom() - computer.zoom * (monitor.y + h)
                                                   : m.y - computer.zoom * monitor.y;
      QString info = QStringLiteral("%1 × %2").arg(w).arg(h);
      if (!result.aspect.empty())
        info += QStringLiteral(" · ") + QString::fromStdString(result.aspect);
      else if (const auto aspect = editor::aspectNameOf(w, h); !aspect.empty() && shift)
        info += QStringLiteral(" · ") + QString::fromStdString(aspect);
      if (result.preset)
        info += QStringLiteral(" · ") + tr("standard size");
      m_drag.info = info;
    }
    m_drag.preview = preview;
    m_drag.previewValid = true;
    m_drag.resizeGuides = result.guides;
    update();
    return;
  }
  if (m_drag.index >= 0) {
    const auto delta = event->position() - m_drag.pressed;
    if (!m_drag.moved && std::hypot(delta.x(), delta.y()) < kDragPixels)
      return;
    m_drag.moved = true;
    m_drag.snapped = snapMove(
        m_arrangement, static_cast<size_t>(m_drag.index), m_drag.originX + delta.x() / m_scale,
        m_drag.originY + delta.y() / m_scale, kSnapPixels / m_scale
    );
    setCursor(Qt::ClosedHandCursor);
    update();
    return;
  }
  // nothing is held: the outline of a computer under the pointer is shown, and the cursor says what a drag does
  const auto hit = outlineAt(event->position());
  const auto hovered =
      hit ? QString::fromStdString(m_arrangement.computers[static_cast<size_t>(hit->index)].name) : QString();
  if (hovered != m_hovered) {
    m_hovered = hovered;
    update();
  }
  if (hit) {
    setCursor(cursorOf(hit->handle));
    return;
  }
  setCursor(computerAt(event->position()).isEmpty() ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void ArrangementCanvas::leaveEvent(QEvent *event)
{
  if (!m_hovered.isEmpty()) {
    m_hovered.clear();
    update();
  }
  QWidget::leaveEvent(event);
}

void ArrangementCanvas::mouseReleaseEvent(QMouseEvent *event)
{
  // another button that is pressed and released while this one is held does not end what this one began
  if (event->button() != m_drag.button) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  const auto drag = m_drag;
  m_drag = {};
  unsetCursor();
  if (drag.resizing) {
    if (drag.moved && drag.previewValid) {
      m_arrangement.computers[static_cast<size_t>(drag.index)] = drag.preview;
      recompute();
      m_needsFit = !m_manualView;
      Q_EMIT arrangementChanged();
      if (drag.scaleMode) {
        Q_EMIT computerScaled(QString::fromStdString(drag.preview.name), drag.preview.zoom);
      } else {
        const auto &r = drag.preview.monitors.front();
        Q_EMIT computerResized(QString::fromStdString(drag.preview.name), r.x, r.y, r.w, r.h);
      }
    }
    update();
    return;
  }
  if (drag.index >= 0 && drag.moved && !drag.snapped.refused) {
    auto &computer = m_arrangement.computers[static_cast<size_t>(drag.index)];
    computer.originX = drag.snapped.originX;
    computer.originY = drag.snapped.originY;
    recompute();
    m_needsFit = !m_manualView;
    Q_EMIT arrangementChanged();
  }
  update();
}

void ArrangementCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
  const auto name = computerAt(event->position());
  if (name.isEmpty())
    return;
  m_drag = {};
  setSelected(name);
  Q_EMIT editRequested(name);
}

void ArrangementCanvas::cancelDrag()
{
  m_drag = {};
  unsetCursor();
}

void ArrangementCanvas::moveSelected(double dx, double dy)
{
  const auto index = indexOf(m_selected);
  if (index < 0)
    return;
  auto &computer = m_arrangement.computers[static_cast<size_t>(index)];
  // keys do not snap, but never put a computer on another
  const auto moved =
      snapMove(m_arrangement, static_cast<size_t>(index), computer.originX + dx, computer.originY + dy, 0);
  if (moved.refused)
    return;
  computer.originX = moved.originX;
  computer.originY = moved.originY;
  recompute();
  m_needsFit = !m_manualView;
  update();
  Q_EMIT arrangementChanged();
}

void ArrangementCanvas::keyPressEvent(QKeyEvent *event)
{
  const double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
  switch (event->key()) {
  case Qt::Key_Left:
    moveSelected(-step, 0);
    break;
  case Qt::Key_Right:
    moveSelected(step, 0);
    break;
  case Qt::Key_Up:
    moveSelected(0, -step);
    break;
  case Qt::Key_Down:
    moveSelected(0, step);
    break;
  case Qt::Key_Escape:
    if (m_drag.index >= 0) {
      cancelDrag();
      update();
      break;
    }
    QWidget::keyPressEvent(event);
    return;
  case Qt::Key_0:
    zoomToFit();
    break;
  case Qt::Key_Plus:
  case Qt::Key_Equal:
    m_scale = std::min(m_scale * 1.25, 2.0);
    m_manualView = true;
    update();
    break;
  case Qt::Key_Minus:
    m_scale = std::max(m_scale / 1.25, 0.002);
    m_manualView = true;
    update();
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }
  event->accept();
}

void ArrangementCanvas::wheelEvent(QWheelEvent *event)
{
  ensureFit();
  const auto steps = event->angleDelta().y();
  if (steps == 0) {
    event->ignore();
    return;
  }
  const auto factor = std::pow(1.0015, steps);
  const auto before = event->position();
  const auto scale = std::clamp(m_scale * factor, 0.002, 2.0);
  // the place under the pointer stays where it is
  const auto at = (before - m_offset) / m_scale;
  m_scale = scale;
  m_offset = before - at * m_scale;
  m_manualView = true;
  update();
  event->accept();
}

void ArrangementCanvas::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  m_needsFit = !m_manualView;
}

bool ArrangementCanvas::event(QEvent *event)
{
  if (event->type() == QEvent::ToolTip) {
    const auto *help = static_cast<QHelpEvent *>(event);
    const auto name = computerAt(help->pos());
    if (name.isEmpty()) {
      QToolTip::hideText();
      return true;
    }
    const auto index = indexOf(name);
    const auto &computer = m_arrangement.computers[static_cast<size_t>(index)];
    QStringList lines;
    lines << tr("%1: %2, zoom %3").arg(name, sizeText(computer)).arg(computer.zoom, 0, 'f', 3);
    for (const auto &issue : m_issues) {
      if (issue.computer == computer.name || issue.other == computer.name)
        lines << describe(issue);
    }
    QToolTip::showText(help->globalPos(), lines.join(QLatin1Char('\n')), this);
    return true;
  }
  return QWidget::event(event);
}

QString ArrangementCanvas::describe(const Issue &issue)
{
  const auto computer = QString::fromStdString(issue.computer);
  const auto other = QString::fromStdString(issue.other);
  using Kind = Issue::Kind;
  switch (issue.kind) {
  case Kind::NameIsEmpty:
    return tr("A computer has no name.");
  case Kind::DuplicateName:
    return tr("%1 and %2 have the same name.").arg(computer, other);
  case Kind::InvalidMonitors:
    return tr("The monitors of %1 are not valid: %2").arg(computer, QString::fromStdString(issue.detail));
  case Kind::InvalidZoom:
    return tr("The zoom of %1 is out of range.").arg(computer);
  case Kind::InvalidPosition:
    return tr("%1 is too far away.").arg(computer);
  case Kind::Overlap:
    return tr("%1 and %2 overlap.").arg(computer, other);
  case Kind::NotConnected:
    return tr("%1 does not touch another computer: the pointer cannot get there.").arg(computer);
  case Kind::EdgeInTheSameRows: {
    const auto side = QString::fromLatin1(directionName(issue.side));
    return tr("%1 has another edge on its %2 side in the same rows as the link to %3, that touches nothing: the "
              "pointer "
              "also leaves there, to %3.")
        .arg(computer, side, other);
  }
  case Kind::AmbiguousSide: {
    const auto side = QString::fromLatin1(directionName(issue.side));
    return tr("%1 has two edges in the same rows of its %2 side that touch computers: %3 is not linked.")
        .arg(computer, side, other);
  }
  }
  return {};
}
