/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TextSize.h"

#include "common/I18N.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QSettings>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace deskflow::gui::style {

namespace {

TextSize *s_instance = nullptr;
double s_metricScale = 1.0;
double s_fontRatio = 1.0;

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

// The weights that the faces of a family have (the italic faces say the same weights again)
QList<int> weightsOfFamily(const QString &family)
{
  QList<int> weights;
  for (const auto &style : QFontDatabase::styles(family)) {
    const int weight = QFontDatabase::weight(family, style);
    if (weight > 0 && !weights.contains(weight))
      weights << weight;
  }
  std::sort(weights.begin(), weights.end());
  return weights;
}

} // namespace

TextSizeEnvironment TextSizeEnvironment::ofThisComputer()
{
  TextSizeEnvironment environment;
  environment.systemFont = [] { return QFontDatabase::systemFont(QFontDatabase::GeneralFont); };
  environment.osTextScale = osTextScaleOfThisComputer;
  environment.platform = platformOfThisComputer();
  environment.language = [] { return I18N::currentLanguage(); };
  environment.fontVariable = [] { return qEnvironmentVariable(kFontFamiliesVariable); };
  environment.hasFamily = [](const QString &family) { return QFontDatabase::hasFamily(family); };
  environment.weightsOf = weightsOfFamily;
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
  setFontWeights({});
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
  // the language of the program changes at once (the settings), and the fonts for its letters with it
  if (m_environment.language)
    connect(I18N::instance(), &I18N::languageChanged, this, [this] { apply(); });
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
  s_fontRatio = 1.0;
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

QString TextSize::describe() const
{
  return QStringLiteral(
             "text size: body %1 px (system font %2 px x OS text size %3, least %4 px%5), step %6 %, controls x%7"
         )
             .arg(m_plan.bodyPixels, 0, 'f', 2)
             .arg(m_input.systemPixels, 0, 'f', 2)
             .arg(m_input.osTextScale, 0, 'f', 2)
             .arg(minimumBodyPixels(m_input.platform), 0, 'f', 0)
             .arg(m_plan.raisedByFloor ? QStringLiteral(", raised") : QString())
             .arg(m_percent)
             .arg(m_plan.metricScale, 0, 'f', 2) +
         (m_fonts.families.isEmpty() ? QString()
                                     : QStringLiteral(", fonts %1 (%2)")
                                           .arg(m_fonts.families.join(QStringLiteral(" + ")))
                                           .arg(
                                               m_fonts.source == FontChoice::Source::Variable
                                                   ? QStringLiteral("%1").arg(QLatin1String(kFontFamiliesVariable))
                                                   : QStringLiteral("language")
                                           ));
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
  return deskflow::gui::style::pixelsPerPoint(QFontInfo(probe).pixelSize(), 288.0);
}

double TextSize::systemPixels(const QFont &font) const
{
  if (font.pointSizeF() > 0)
    return font.pointSizeF() * pixelsPerPoint(font);
  if (font.pixelSize() > 0)
    return font.pixelSize();
  return QFontInfo(font).pixelSize();
}

FontChoice TextSize::fontsForNow() const
{
  FontPolicyInput input;
  input.platform = m_environment.platform;
  input.language = m_environment.language ? m_environment.language() : QString();
  input.systemFamily = m_system.family();
  input.variable = m_environment.fontVariable ? m_environment.fontVariable() : QString();
  input.hasFamily = m_environment.hasFamily;
  input.weightsOf = m_environment.weightsOf;
  return chooseFonts(input);
}

void TextSize::apply()
{
  TextSizeInput input;
  input.platform = m_environment.platform;
  input.osTextScale = m_environment.osTextScale ? m_environment.osTextScale() : 1.0;
  input.systemPixels = systemPixels(m_system);
  input.percent = m_percent;
  const auto plan = planTextSize(input);
  m_input = input;

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

  // the fonts for the letters that the system font does not have (Hangul): the families of the application font, and
  // the weights that its roles get
  const auto fonts = fontsForNow();
  const bool fontsChanged = fonts.families != m_fonts.families || fonts.weights != m_fonts.weights;
  m_fonts = fonts;
  if (!fonts.families.isEmpty())
    target.setFamilies(fonts.families);
  setFontWeights(fonts.weights);

  const bool changed = fontsChanged || std::abs(plan.metricScale - m_plan.metricScale) > 1e-9 ||
                       std::abs(plan.bodyPixels - m_plan.bodyPixels) > 1e-9;
  m_plan = plan;
  // the sizes of the controls come first: a widget that is told of the font asks for them
  s_metricScale = plan.metricScale;
  s_fontRatio = sameAsSystem || input.systemPixels <= 0 ? 1.0 : plan.bodyPixels / input.systemPixels;
  const bool ours = !sameAsSystem || !fonts.families.isEmpty();
  if (ours || m_fontIsOurs) {
    if (QApplication::font() != target)
      setApplicationFont(target);
    m_fontIsOurs = ours;
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

double fontRatio()
{
  return s_fontRatio;
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
