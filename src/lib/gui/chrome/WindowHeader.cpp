/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "WindowHeader.h"

#include "gui/style/Appearance.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PixelSnap.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace deskflow::gui::chrome {

using namespace deskflow::gui::style;

namespace {

// the size of the glyph of a button of the window, the size of the ones of Windows 11
constexpr int kGlyph = 10;
constexpr int kMoreWidth = 40;
constexpr int kMoreHeight = 32;
constexpr int kMoreIcon = 20;

// the font of a role: the size that the system gives text, as many times as the role is larger than the body
QFont scaled(const QWidget *widget, int pixels, QFont::Weight weight)
{
  auto font = scaleFont(QApplication::font(widget), qreal(pixels) / metrics::fontBody);
  font.setWeight(weight);
  return font;
}

} // namespace

HeaderButton::HeaderButton(Glyph glyph, QWidget *parent) : QAbstractButton(parent), m_glyph(glyph)
{
  setAttribute(Qt::WA_Hover);
  // the buttons of the window are not stops of the keyboard (the keys of the system do what they do: Alt+Space,
  // Win+Up), the button of the menu is
  setFocusPolicy(glyph == Glyph::More ? Qt::StrongFocus : Qt::NoFocus);
  new LookFollower(this, [this] { update(); });
}

HeaderButton::Glyph HeaderButton::glyph() const
{
  return m_glyph;
}

void HeaderButton::setGlyph(Glyph glyph)
{
  if (m_glyph == glyph)
    return;
  m_glyph = glyph;
  update();
}

void HeaderButton::setNativeState(bool hover, bool pressed)
{
  if (m_nativeHover == hover && m_nativePressed == pressed)
    return;
  m_nativeHover = hover;
  m_nativePressed = pressed;
  update();
}

QSize HeaderButton::sizeHint() const
{
  return m_glyph == Glyph::More ? QSize(kMoreWidth, kMoreHeight)
                                : QSize(WindowHeader::kCaptionButtonWidth, WindowHeader::kHeight);
}

QSize HeaderButton::minimumSizeHint() const
{
  return sizeHint();
}

void HeaderButton::keyPressEvent(QKeyEvent *event)
{
  // a button of Qt answers to the space bar; the button of the menu answers to Enter as well, as a button of a toolbar
  // of the system does (the buttons of the window are no stops of the keyboard, they never get a key)
  if (m_glyph == Glyph::More && !event->isAutoRepeat() &&
      (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
    click();
    event->accept();
    return;
  }
  QAbstractButton::keyPressEvent(event);
}

void HeaderButton::focusInEvent(QFocusEvent *event)
{
  m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                    event->reason() == Qt::ShortcutFocusReason;
  QAbstractButton::focusInEvent(event);
  update();
}

void HeaderButton::focusOutEvent(QFocusEvent *event)
{
  m_keyboardFocus = false;
  QAbstractButton::focusOutEvent(event);
  update();
}

void HeaderButton::paintEvent(QPaintEvent *)
{
  const auto tokens = currentTokens();
  const auto &c = tokens.colors;
  QPainter painter(this);
  const auto dpr = painter.device()->devicePixelRatioF();
  const bool hover = (underMouse() || m_nativeHover) && isEnabled();
  const bool pressed = (isDown() || m_nativePressed) && isEnabled();
  const bool close = m_glyph == Glyph::Close;
  const bool more = m_glyph == Glyph::More;

  if (!isEnabled())
    painter.setOpacity(0.45);

  // the background that shows where the mouse is: the whole button for the buttons of the window (the close button is
  // red, as it is in the window of the system), a rounded shape for the menu
  QColor glyphColor = c.text;
  if (hover || pressed) {
    QColor fill = c.text;
    fill.setAlphaF(pressed ? 0.14 : 0.08);
    if (close) {
      fill = c.danger;
      fill.setAlphaF(pressed ? 0.85 : 1.0);
      glyphColor = QColor(255, 255, 255);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    if (more) {
      painter.setRenderHint(QPainter::Antialiasing);
      const QRectF shape = snapRect(QRectF(rect()), dpr);
      painter.drawRoundedRect(shape, 8, 8);
      painter.setRenderHint(QPainter::Antialiasing, false);
    } else {
      painter.drawRect(rect());
    }
  }

  if (more) {
    const auto pixmap =
        iconPixmap(QStringLiteral("dots-three"), IconWeight::Regular, glyphColor, QSize(kMoreIcon, kMoreIcon), dpr);
    painter.drawPixmap(QRect((width() - kMoreIcon) / 2, (height() - kMoreIcon) / 2, kMoreIcon, kMoreIcon), pixmap);
  } else {
    // the glyph is drawn with lines of one device pixel that are on whole device pixels, as the system draws its own
    const auto line = deviceLineWidth(1, dpr);
    const auto at = [&](qreal logical) { return snapToDevice(logical, dpr) + line / 2; };
    const qreal cx = std::round(width() / 2.0);
    const qreal cy = std::round(height() / 2.0);
    const qreal half = kGlyph / 2.0;
    painter.setPen(QPen(glyphColor, line));
    painter.setBrush(Qt::NoBrush);
    switch (m_glyph) {
    case Glyph::Minimize:
      painter.drawLine(QPointF(at(cx - half), at(cy)), QPointF(at(cx + half), at(cy)));
      break;
    case Glyph::Maximize:
      painter.drawRect(QRectF(at(cx - half), at(cy - half), kGlyph - line, kGlyph - line));
      break;
    case Glyph::Restore: {
      // a square in front, and the corner of another one behind it
      painter.drawRect(QRectF(at(cx - half), at(cy - half + 2), kGlyph - 2 - line, kGlyph - 2 - line));
      QPainterPath behind;
      behind.moveTo(at(cx - half + 2), at(cy - half + 2) - line);
      behind.lineTo(at(cx - half + 2), at(cy - half));
      behind.lineTo(at(cx + half) - line, at(cy - half));
      behind.lineTo(at(cx + half) - line, at(cy + half - 2) - line);
      behind.lineTo(at(cx + half - 2), at(cy + half - 2) - line);
      painter.drawPath(behind);
      break;
    }
    case Glyph::Close:
      painter.setRenderHint(QPainter::Antialiasing);
      painter.drawLine(QPointF(cx - half, cy - half), QPointF(cx + half, cy + half));
      painter.drawLine(QPointF(cx - half, cy + half), QPointF(cx + half, cy - half));
      break;
    case Glyph::More:
      break;
    }
  }

  if (m_keyboardFocus) {
    const auto ring = deviceLineWidth(metrics::focusRingWidth, dpr);
    const QRectF outer = QRectF(rect()).adjusted(ring / 2, ring / 2, -ring / 2, -ring / 2);
    painter.setOpacity(1.0);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(c.accent, ring));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(outer, more ? 8 : 0, more ? 8 : 0);
  }
}

// ---------------------------------------------------------------------------------------------------------------------

WindowHeader::WindowHeader(const Options &options, QWidget *parent)
    : QWidget(parent),
      m_options(options),
      m_title(new QLabel(this)),
      m_more(new HeaderButton(HeaderButton::Glyph::More, this))
{
  setObjectName(QStringLiteral("windowHeader"));
  setFixedHeight(kHeight);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(options.leftInset > 0 ? options.leftInset : metrics::spaceXl, 0, 0, 0);
  layout->setSpacing(0);

  m_title->setObjectName(QStringLiteral("windowHeaderTitle"));
  m_title->setTextFormat(Qt::PlainText);
  m_title->setTextInteractionFlags(Qt::NoTextInteraction);
  // a press on the name is a press on the header (the part that drags the window)
  m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_title->setVisible(true);
  layout->addWidget(m_title, 1, Qt::AlignVCenter);

  m_more->setObjectName(QStringLiteral("windowHeaderMore"));
  layout->addWidget(m_more, 0, Qt::AlignVCenter);
  layout->addSpacing(metrics::spaceXs);
  connect(m_more, &QAbstractButton::clicked, this, &WindowHeader::openMenu);

  if (options.captionButtons) {
    m_minimize = new HeaderButton(HeaderButton::Glyph::Minimize, this);
    m_minimize->setObjectName(QStringLiteral("captionMinimize"));
    m_maximize = new HeaderButton(HeaderButton::Glyph::Maximize, this);
    m_maximize->setObjectName(QStringLiteral("captionMaximize"));
    m_close = new HeaderButton(HeaderButton::Glyph::Close, this);
    m_close->setObjectName(QStringLiteral("captionClose"));
    for (auto *button : {m_minimize, m_maximize, m_close})
      layout->addWidget(button, 0, Qt::AlignTop);
    connect(m_minimize, &QAbstractButton::clicked, this, &WindowHeader::minimizeRequested);
    connect(m_maximize, &QAbstractButton::clicked, this, &WindowHeader::maximizeToggleRequested);
    connect(m_close, &QAbstractButton::clicked, this, &WindowHeader::closeRequested);
  }

  new LookFollower(this, [this] {
    const auto &c = currentTokens().colors;
    auto palette = m_title->palette();
    palette.setColor(QPalette::WindowText, c.textSecondary);
    m_title->setPalette(palette);
    m_title->setFont(scaled(this, metrics::fontBody, QFont::DemiBold));
  });
  retranslate();
}

WindowHeader::Options WindowHeader::options() const
{
  return m_options;
}

void WindowHeader::setTitle(const QString &title)
{
  m_title->setText(m_options.title ? title : QString());
  setAccessibleName(title);
}

QString WindowHeader::title() const
{
  return m_title->text();
}

void WindowHeader::setMenu(QMenu *menu)
{
  m_menu = menu;
}

QMenu *WindowHeader::menu() const
{
  return m_menu;
}

void WindowHeader::setMaximized(bool maximized)
{
  if (m_maximized == maximized)
    return;
  m_maximized = maximized;
  if (m_maximize)
    m_maximize->setGlyph(maximized ? HeaderButton::Glyph::Restore : HeaderButton::Glyph::Maximize);
  retranslate();
}

bool WindowHeader::isMaximized() const
{
  return m_maximized;
}

HeaderButton *WindowHeader::moreButton() const
{
  return m_more;
}

HeaderButton *WindowHeader::captionButton(Region region) const
{
  switch (region) {
  case Region::Minimize:
    return m_minimize;
  case Region::Maximize:
    return m_maximize;
  case Region::Close:
    return m_close;
  default:
    return nullptr;
  }
}

QList<QRect> WindowHeader::interactiveRects() const
{
  return {m_more->geometry()};
}

void WindowHeader::retranslate()
{
  const auto name = [](HeaderButton *button, const QString &text) {
    if (!button)
      return;
    button->setAccessibleName(text);
    button->setToolTip(text);
  };
  name(m_more, tr("More"));
  name(m_minimize, tr("Minimize"));
  name(m_maximize, m_maximized ? tr("Restore") : tr("Maximize"));
  name(m_close, tr("Close"));
}

void WindowHeader::openMenu()
{
  if (!m_menu)
    return;
  const int width = m_menu->sizeHint().width();
  m_menu->popup(m_more->mapToGlobal(QPoint(m_more->width() - width, m_more->height() + metrics::spaceXs)));
}

void WindowHeader::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    Q_EMIT dragRequested();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void WindowHeader::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    Q_EMIT captionDoubleClicked();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

void WindowHeader::contextMenuEvent(QContextMenuEvent *event)
{
  Q_EMIT systemMenuRequested(event->globalPos());
  event->accept();
}

} // namespace deskflow::gui::chrome
