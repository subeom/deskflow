/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TextSize.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QSettings>
#include <QWidget>

#include <cmath>

namespace deskflow::gui::style {

namespace {

TextSize *s_instance = nullptr;
double s_metricScale = 1.0;

TextPlatform platformOfThisComputer()
{
  if (deskflow::platform::isWindows())
    return TextPlatform::Windows;
  if (deskflow::platform::isMac())
    return TextPlatform::Mac;
  if (QSysInfo::kernelType() == QStringLiteral("linux"))
    return TextPlatform::Linux;
  return TextPlatform::Other;
}

double osTextScaleOfThisComputer()
{
#ifdef Q_OS_WIN
  // Settings > Accessibility > Text size: the percent is in the registry, and Qt does not take it for the font of the
  // system (that has the size of the message font of the non-client metrics)
  const QSettings accessibility(
      QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Accessibility"), QSettings::NativeFormat
  );
  return osTextScaleFromPercent(accessibility.value(QStringLiteral("TextScaleFactor"), 100).toInt());
#else
  return 1.0;
#endif
}

// Qt tells the windows of a new font of the application from the event loop, some time after; the ones that exist are
// told now, so that the size is the same everywhere when this returns (the event comes again, which does no harm)
void setApplicationFont(const QFont &font)
{
  QApplication::setFont(font);
  QEvent event(QEvent::ApplicationFontChange);
  for (auto *widget : QApplication::allWidgets()) {
    if (widget->isWindow())
      QCoreApplication::sendEvent(widget, &event);
  }
}

} // namespace

TextSizeEnvironment TextSizeEnvironment::ofThisComputer()
{
  TextSizeEnvironment environment;
  environment.systemFont = [] { return QFontDatabase::systemFont(QFontDatabase::GeneralFont); };
  environment.osTextScale = osTextScaleOfThisComputer;
  environment.platform = platformOfThisComputer();
  return environment;
}

TextSize *TextSize::instance()
{
  return s_instance;
}

TextSize *TextSize::install(const TextSizeEnvironment &environment)
{
  uninstall();
  s_instance = new TextSize(environment);
  s_instance->apply();
  return s_instance;
}

void TextSize::uninstall()
{
  if (!s_instance)
    return;
  // the font of the system goes back while the application can still tell its widgets
  if (s_instance->m_fontIsOurs)
    setApplicationFont(s_instance->m_system);
  s_instance->m_fontIsOurs = false;
  delete s_instance;
  // the controls are as they were drawn again
  Q_EMIT TextSizeNotifier::instance()->changed();
}

TextSize::TextSize(const TextSizeEnvironment &environment) : QObject(qApp), m_environment(environment)
{
  s_instance = this;
  m_system = m_environment.systemFont ? m_environment.systemFont() : QApplication::font();
  m_percent = snapTextPercent(Settings::value(Settings::Gui::TextSize).toInt());

  connect(Settings::instance(), &Settings::settingsChanged, this, &TextSize::followSetting);
  // the person changed the size of text or the font in the settings of the OS, and came back
  connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
    if (state == Qt::ApplicationActive)
      refresh();
  });
}

TextSize::~TextSize()
{
  if (s_instance == this)
    s_instance = nullptr;
  s_metricScale = 1.0;
}

TextSizePlan TextSize::plan() const
{
  return m_plan;
}

int TextSize::percent() const
{
  return m_percent;
}

QFont TextSize::systemFont() const
{
  return m_system;
}

void TextSize::setPercent(int percent)
{
  const auto snapped = snapTextPercent(percent);
  if (Settings::value(Settings::Gui::TextSize).toInt() != snapped)
    Settings::setValue(Settings::Gui::TextSize, snapped);
  // the setting reaches followSetting; without a writable setting the step is still taken
  if (m_percent != snapped) {
    m_percent = snapped;
    apply();
  }
}

void TextSize::step(int direction)
{
  setPercent(stepTextPercent(m_percent, direction));
}

void TextSize::refresh()
{
  if (m_environment.systemFont)
    m_system = m_environment.systemFont();
  apply();
}

void TextSize::followSetting(const QString &key)
{
  if (key != Settings::Gui::TextSize)
    return;
  const auto percent = snapTextPercent(Settings::value(Settings::Gui::TextSize).toInt());
  if (percent == m_percent)
    return;
  m_percent = percent;
  apply();
}

double TextSize::pixelsPerPoint(const QFont &font) const
{
  // the size of a point in pixels is that of the screen the font is made for: asked of the font itself, with a size
  // that is large enough to be exact
  auto probe = font;
  probe.setPointSizeF(288.0);
  const auto pixels = QFontInfo(probe).pixelSize();
  return pixels > 0 ? pixels / 288.0 : 96.0 / 72.0;
}

double TextSize::systemPixels(const QFont &font) const
{
  if (font.pointSizeF() > 0)
    return font.pointSizeF() * pixelsPerPoint(font);
  if (font.pixelSize() > 0)
    return font.pixelSize();
  return QFontInfo(font).pixelSize();
}

void TextSize::apply()
{
  TextSizeInput input;
  input.platform = m_environment.platform;
  input.osTextScale = m_environment.osTextScale ? m_environment.osTextScale() : 1.0;
  input.systemPixels = systemPixels(m_system);
  input.percent = m_percent;
  const auto plan = planTextSize(input);

  // the font of the application is the one of the system, but for its size; with the size of the system it is not
  // touched (a Mac that has the size that the settings of macOS have keeps its font as it is)
  QFont target = m_system;
  const bool sameAsSystem = std::abs(plan.bodyPixels - input.systemPixels) < 0.005;
  if (!sameAsSystem) {
    if (m_system.pointSizeF() > 0)
      target.setPointSizeF(plan.bodyPixels / pixelsPerPoint(m_system));
    else
      target.setPixelSize(std::max(1, static_cast<int>(std::lround(plan.bodyPixels))));
  }

  const bool changed =
      std::abs(plan.metricScale - m_plan.metricScale) > 1e-9 || std::abs(plan.bodyPixels - m_plan.bodyPixels) > 1e-9;
  m_plan = plan;
  // the sizes of the controls come first: a widget that is told of the font asks for them
  s_metricScale = plan.metricScale;
  if (!sameAsSystem || m_fontIsOurs) {
    if (QApplication::font() != target)
      setApplicationFont(target);
    m_fontIsOurs = !sameAsSystem;
  }
  if (changed) {
    Q_EMIT planChanged(plan);
    Q_EMIT TextSizeNotifier::instance()->changed();
  }
}

TextSizeNotifier *TextSizeNotifier::instance()
{
  static QPointer<TextSizeNotifier> notifier;
  if (!notifier)
    notifier = new TextSizeNotifier(qApp);
  return notifier;
}

QMetaObject::Connection onTextSizeChanged(const QObject *context, const std::function<void()> &callback)
{
  return QObject::connect(TextSizeNotifier::instance(), &TextSizeNotifier::changed, context, callback);
}

double metricScale()
{
  return s_metricScale;
}

int px(int logicalPixels)
{
  return scaledPixels(logicalPixels, s_metricScale);
}

} // namespace deskflow::gui::style
