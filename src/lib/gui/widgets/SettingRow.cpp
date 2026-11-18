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

} // namespace

SettingRow::SettingRow(const QString &title, const QString &description, QWidget *parent)
    : QWidget(parent),
      m_title(new QLabel(this)),
      m_hint(new InlineHint(this)),
      m_texts(new QWidget(this)),
      m_layout(new QHBoxLayout(this)),
      m_controlLayout(new QHBoxLayout)
{
  installAccessibleInterfaces();
  // the row is named by its title and described by its hint: the two texts are not walked to again below it
  markRedundantForScreenReader(m_title);
  markRedundantForScreenReader(m_hint);
  setMinimumHeight(kMinimumHeight);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  m_title->setWordWrap(true);
  m_title->setTextInteractionFlags(Qt::NoTextInteraction);
  m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  auto *texts = new QVBoxLayout(m_texts);
  texts->setContentsMargins(0, 0, 0, 0);
  texts->setSpacing(kTextGap);
  texts->addWidget(m_title);
  texts->addWidget(m_hint);
  m_texts->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  m_controlLayout->setContentsMargins(0, 0, 0, 0);
  m_controlLayout->setSpacing(kControlGap);

  m_layout->setContentsMargins(metrics::spaceXl, kVerticalPadding, metrics::spaceXl, kVerticalPadding);
  m_layout->setSpacing(kColumnGap);
  m_layout->addWidget(m_texts, 1, Qt::AlignVCenter);
  m_layout->addLayout(m_controlLayout);

  setTitle(title);
  setDescription(description);
  new LookFollower(this, [this] { applyLook(); });
}

int SettingRow::minimumRowHeight()
{
  return kMinimumHeight;
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
  m_texts->setSizePolicy(expanding ? QSizePolicy::Preferred : QSizePolicy::Expanding, QSizePolicy::Minimum);
  for (auto *control : std::as_const(m_controls))
    m_controlLayout->setStretchFactor(control, expanding ? 1 : 0);
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
  return m_texts->geometry();
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
  return toggle && toggle->isEnabled() && m_texts->geometry().contains(pos);
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
  auto font = m_title->font();
  font.setWeight(QFont::Medium);
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
