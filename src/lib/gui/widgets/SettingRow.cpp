/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingRow.h"

#include "InlineHint.h"
#include "ToggleSwitch.h"
#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PixelSnap.h"
#include "gui/style/TextSize.h"

#include <QAbstractButton>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

using namespace deskflow::gui::style;

namespace {

constexpr int kMinimumHeight = 44;
constexpr int kVerticalPadding = 10;
constexpr int kControlGap = 10;
constexpr int kTextGap = 2;
// the room between the text and the controls
constexpr int kColumnGap = 16;

// The lines of text, centred in the height that they are given as a widget is that is aligned in a layout. A layout of
// Qt that is aligned would give lines that wrap less height than they need at their width.
class CenteredColumn : public QVBoxLayout
{
public:
  using QVBoxLayout::QVBoxLayout;

  void setGeometry(const QRect &rect) override
  {
    const int needed = hasHeightForWidth() ? heightForWidth(rect.width()) : sizeHint().height();
    const int height = qMin(rect.height(), needed);
    QVBoxLayout::setGeometry(QRect(rect.x(), rect.y() + (rect.height() - height) / 2, rect.width(), height));
  }
};

// The layout of a row: the text and the controls side by side, as a box has them. A row that may stack puts the
// controls under the text, at the full width of the row, when the two do not fit side by side (the text is given a
// width that reads well first). What the row asks for is then the wider of the two, and not their sum: a text field and
// a button do not make a window wider than it promised when the text is larger.
class RowLayout : public QHBoxLayout
{
public:
  using QHBoxLayout::QHBoxLayout;

  void setParts(QLayout *text, QLayout *controls)
  {
    m_text = text;
    m_controls = controls;
  }

  void setStackWhenNarrow(bool stack)
  {
    if (m_stack == stack)
      return;
    m_stack = stack;
    invalidate();
  }

  bool stackWhenNarrow() const
  {
    return m_stack;
  }

  QSize minimumSize() const override
  {
    const auto box = QHBoxLayout::minimumSize();
    if (!m_stack || !m_text || !m_controls)
      return box;
    const auto margins = effectiveMargins();
    return QSize(
        margins.left() + margins.right() + qMax(m_text->minimumSize().width(), m_controls->minimumSize().width()),
        box.height()
    );
  }

  bool hasHeightForWidth() const override
  {
    return (m_stack && m_text && m_controls) || QHBoxLayout::hasHeightForWidth();
  }

  int heightForWidth(int width) const override
  {
    if (!m_stack || !m_text || !m_controls)
      return QHBoxLayout::heightForWidth(width);
    const auto margins = effectiveMargins();
    const int inner = width - margins.left() - margins.right();
    if (sideBySideAt(inner))
      return QHBoxLayout::heightForWidth(width);
    return margins.top() + margins.bottom() + textHeight(inner) + kStackGap + controlsHeight();
  }

  int minimumHeightForWidth(int width) const override
  {
    if (!m_stack || !m_text || !m_controls)
      return QHBoxLayout::minimumHeightForWidth(width);
    return heightForWidth(width);
  }

  void setGeometry(const QRect &rect) override
  {
    if (!m_stack || !m_text || !m_controls) {
      QHBoxLayout::setGeometry(rect);
      return;
    }
    const auto margins = effectiveMargins();
    const int inner = rect.width() - margins.left() - margins.right();
    if (sideBySideAt(inner)) {
      QHBoxLayout::setGeometry(rect);
      return;
    }
    QLayout::setGeometry(rect);
    const QRect area = rect.marginsRemoved(margins);
    const int textHeight = qMin(area.height(), this->textHeight(area.width()));
    m_text->setGeometry(QRect(area.x(), area.y(), area.width(), textHeight));
    const int top = area.y() + textHeight + kStackGap;
    m_controls->setGeometry(QRect(area.x(), top, area.width(), qMax(0, qMin(controlsHeight(), area.bottom() + 1 - top)))
    );
  }

private:
  // the room between the text and the controls when they are one above the other
  static constexpr int kStackGap = 8;
  // the width that the text has when it is beside the controls, at the least: a column that is narrower than this wraps
  // the guidance into lines of a few letters
  static constexpr int kComfortableText = 200;

  QMargins effectiveMargins() const
  {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    return QMargins(left, top, right, bottom);
  }

  bool sideBySideAt(int innerWidth) const
  {
    const int text = qMax(m_text->minimumSize().width(), qMin(m_text->sizeHint().width(), px(kComfortableText)));
    return innerWidth >= text + spacing() + m_controls->minimumSize().width();
  }

  int textHeight(int width) const
  {
    return m_text->hasHeightForWidth() ? m_text->heightForWidth(width) : m_text->sizeHint().height();
  }

  int controlsHeight() const
  {
    return qMax(m_controls->minimumSize().height(), m_controls->sizeHint().height());
  }

  QLayout *m_text = nullptr;
  QLayout *m_controls = nullptr;
  bool m_stack = false;
};

} // namespace

SettingRow::SettingRow(const QString &title, const QString &description, QWidget *parent)
    : QWidget(parent),
      m_title(new QLabel(this)),
      m_hint(new InlineHint(this)),
      m_layout(new RowLayout(this)),
      m_textLayout(new CenteredColumn),
      m_controlLayout(new QHBoxLayout)
{
  installAccessibleInterfaces();
  // the row is named by its title and described by its hint: the two texts are not walked to again below it
  markRedundantForScreenReader(m_title);
  markRedundantForScreenReader(m_hint);
  setMinimumHeight(minimumRowHeight());
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  m_title->setWordWrap(true);
  m_title->setTextInteractionFlags(Qt::NoTextInteraction);
  m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  // The title and the hint are the children of the row, as the controls are, and not of a widget of their own: Qt
  // before 6.11 finds the label of a control (the relation that a screen reader asks for, which names a combo box on
  // Unix) among the siblings of the control only.
  m_textLayout->setContentsMargins(0, 0, 0, 0);
  m_textLayout->setSpacing(kTextGap);
  m_textLayout->addWidget(m_title);
  m_textLayout->addWidget(m_hint);

  m_controlLayout->setContentsMargins(0, 0, 0, 0);
  m_controlLayout->setSpacing(kControlGap);

  m_layout->setContentsMargins(metrics::spaceXl, kVerticalPadding, metrics::spaceXl, kVerticalPadding);
  m_layout->setSpacing(kColumnGap);
  m_layout->addLayout(m_textLayout, 1);
  m_layout->addLayout(m_controlLayout);
  static_cast<RowLayout *>(m_layout)->setParts(m_textLayout, m_controlLayout);

  setTitle(title);
  setDescription(description);
  new LookFollower(this, [this] { applyLook(); });
}

int SettingRow::minimumRowHeight()
{
  return px(kMinimumHeight);
}

QString SettingRow::rawTitle() const
{
  return m_rawTitle;
}

QString SettingRow::title() const
{
  return withoutMnemonic(m_rawTitle);
}

void SettingRow::setTitle(const QString &title)
{
  m_rawTitle = title;
  m_title->setText(title);
  m_title->setVisible(!title.isEmpty());
  setAccessibleName(this->title());
  updateNames();
}

QString SettingRow::description() const
{
  return m_description;
}

void SettingRow::setDescription(const QString &description)
{
  m_description = description;
  showHint();
}

QString SettingRow::error() const
{
  return m_error;
}

void SettingRow::setError(const QString &error)
{
  m_error = error;
  showHint();
}

void SettingRow::showHint()
{
  // an error takes the place of the guidance, and is what a screen reader gets as the description
  const auto text = m_error.isEmpty() ? m_description : m_error;
  m_hint->setText(text);
  m_hint->setVisible(!text.isEmpty());
  m_hint->setDanger(!m_error.isEmpty());
  updateNames();
}

void SettingRow::addControl(QWidget *control)
{
  if (auto *toggle = qobject_cast<ToggleSwitch *>(control);
      toggle && m_rawTitle.isEmpty() && !toggle->text().isEmpty()) {
    setTitle(toggle->text());
    toggle->setText({});
  }

  if (toolTip().isEmpty())
    setToolTip(control->toolTip());
  // the help of Shift+F1 is reached from the text of the row as well
  if (whatsThis().isEmpty())
    setWhatsThis(control->whatsThis());

  m_controls.append(control);
  m_controlLayout->addWidget(control, 0, Qt::AlignVCenter);
  control->installEventFilter(this);
  updateNames();
  updateState();
}

QList<QWidget *> SettingRow::controls() const
{
  return m_controls;
}

QWidget *SettingRow::primaryControl() const
{
  return m_controls.isEmpty() ? nullptr : m_controls.last();
}

void SettingRow::setExpandingControls(bool expanding)
{
  m_layout->setStretch(0, expanding ? 0 : 1);
  m_layout->setStretch(1, expanding ? 1 : 0);
  // the text takes the room that is left over only when the controls do not
  const auto policy = expanding ? QSizePolicy::Preferred : QSizePolicy::Expanding;
  m_title->setSizePolicy(policy, QSizePolicy::Minimum);
  m_hint->setSizePolicy(policy, QSizePolicy::Minimum);
  for (auto *control : std::as_const(m_controls))
    m_controlLayout->setStretchFactor(control, expanding ? 1 : 0);
}

void SettingRow::setStackWhenNarrow(bool stack)
{
  static_cast<RowLayout *>(m_layout)->setStackWhenNarrow(stack);
}

bool SettingRow::stackWhenNarrow() const
{
  return static_cast<RowLayout *>(m_layout)->stackWhenNarrow();
}

bool SettingRow::hasDivider() const
{
  return m_divider;
}

void SettingRow::setDivider(bool divider)
{
  if (m_divider == divider)
    return;
  m_divider = divider;
  update();
}

QRect SettingRow::textRect() const
{
  // the column that the text has in the row, once the layout has given it one
  const auto column = m_textLayout->geometry();
  if (column.isValid())
    return column;
  QRect rect;
  for (const QWidget *line : {static_cast<const QWidget *>(m_title), static_cast<const QWidget *>(m_hint)}) {
    if (!line->isHidden())
      rect = rect.united(line->geometry());
  }
  return rect;
}

void SettingRow::updateNames()
{
  const auto name = title();
  QWidget *first = m_controls.isEmpty() ? nullptr : m_controls.first();
  for (auto *control : std::as_const(m_controls)) {
    // a button that has a text (a radio) is named by it; a control that a form already named keeps its name
    auto *button = qobject_cast<QAbstractButton *>(control);
    const bool named = button && !button->text().isEmpty();
    if (!named && (control->accessibleName().isEmpty() || control->property("settingRowName").toBool())) {
      control->setAccessibleName(name);
      control->setProperty("settingRowName", true);
    }
  }
  if (first && (first->accessibleDescription().isEmpty() || first->property("settingRowDescription").toBool())) {
    first->setAccessibleDescription(m_error.isEmpty() ? description() : m_error);
    first->setProperty("settingRowDescription", true);
  }
  if (auto *primary = primaryControl())
    m_title->setBuddy(primary->focusPolicy() != Qt::NoFocus ? primary : nullptr);
}

void SettingRow::updateState()
{
  bool shown = false;
  bool enabled = false;
  for (const auto *control : std::as_const(m_controls)) {
    if (control->isHidden())
      continue;
    shown = true;
    enabled = enabled || control->isEnabled();
  }

  // the text is dimmed by the same means as the controls: disabled
  m_title->setEnabled(enabled || m_controls.isEmpty());
  m_hint->setEnabled(enabled || m_controls.isEmpty());
  if (m_controls.isEmpty())
    return;

  // only a row that this hid is shown again: what the form hid or shows by itself is left to it
  if (!shown && !m_hiddenByControls) {
    m_hiddenByControls = true;
    hide();
  } else if (shown && m_hiddenByControls) {
    m_hiddenByControls = false;
    show();
  }
}

bool SettingRow::eventFilter(QObject *watched, QEvent *event)
{
  if (m_controls.contains(qobject_cast<QWidget *>(watched))) {
    switch (event->type()) {
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::EnabledChange:
      updateState();
      break;
    default:
      break;
    }
  }
  return QWidget::eventFilter(watched, event);
}

bool SettingRow::clickOnText(const QPoint &pos) const
{
  auto *toggle = qobject_cast<ToggleSwitch *>(primaryControl());
  return toggle && toggle->isEnabled() && textRect().contains(pos);
}

void SettingRow::mousePressEvent(QMouseEvent *event)
{
  m_pressedOnText = event->button() == Qt::LeftButton && clickOnText(event->position().toPoint());
  if (m_pressedOnText)
    event->accept();
  else
    QWidget::mousePressEvent(event);
}

void SettingRow::mouseReleaseEvent(QMouseEvent *event)
{
  const bool click = m_pressedOnText && event->button() == Qt::LeftButton && clickOnText(event->position().toPoint());
  m_pressedOnText = false;
  if (!click) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  auto *toggle = qobject_cast<ToggleSwitch *>(primaryControl());
  toggle->setFocus(Qt::MouseFocusReason);
  toggle->click();
  event->accept();
}

void SettingRow::applyLook()
{
  setMinimumHeight(minimumRowHeight());
  auto font = m_title->font();
  font.setWeight(fontWeight(QFont::Medium));
  m_title->setFont(font);

  auto palette = m_title->palette();
  const auto &c = currentTokens().colors;
  palette.setColor(QPalette::WindowText, c.text);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, c.textDisabled);
  m_title->setPalette(palette);
  update();
}

void SettingRow::paintEvent(QPaintEvent *)
{
  if (!m_divider)
    return;

  QPainter painter(this);
  const auto dpr = painter.device()->devicePixelRatioF();
  auto color = currentTokens().colors.text;
  color.setAlphaF(0.12);
  const auto line = deviceLineWidth(1, dpr);
  painter.fillRect(QRectF(metrics::spaceXl, snapToDevice(0, dpr), width() - 2 * metrics::spaceXl, line), color);
}
