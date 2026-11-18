/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TextSizeTests.h"

#include "common/Settings.h"
#include "gui/style/Appearance.h"
#include "gui/style/FontScale.h"
#include "gui/style/LookFollower.h"
#include "gui/style/TextSize.h"

#include <QApplication>
#include <QEvent>
#include <QFontInfo>
#include <QLabel>
#include <QSignalSpy>
#include <QWidget>

using namespace deskflow::gui::style;

namespace {

QFont fontOfPixels(int pixels)
{
  QFont font = QApplication::font();
  font.setPixelSize(pixels);
  return font;
}

// The system as the test makes it up: the font in pixels, the OS, and what it says about the size of text
struct FakeSystem
{
  int pixels = 12;
  double osScale = 1.0;
  TextPlatform platform = TextPlatform::Windows;

  TextSizeEnvironment environment()
  {
    TextSizeEnvironment env;
    env.systemFont = [this] { return fontOfPixels(pixels); };
    env.osTextScale = [this] { return osScale; };
    env.platform = platform;
    return env;
  }
};

int appFontPixels()
{
  return QFontInfo(QApplication::font()).pixelSize();
}

// Counts the QEvent::ApplicationFontChange that a widget is sent
class FontWatcher : public QWidget
{
public:
  int changes = 0;

protected:
  bool event(QEvent *event) override
  {
    if (event->type() == QEvent::ApplicationFontChange)
      ++changes;
    return QWidget::event(event);
  }
};

} // namespace

void TextSizeTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
}

void TextSizeTests::init()
{
  qputenv("DESKFLOW_APPEARANCE", "");
  qunsetenv("DESKFLOW_APPEARANCE");
  Settings::setValue(Settings::Gui::TextSize);
  Settings::setValue(Settings::Gui::Appearance);
}

void TextSizeTests::cleanup()
{
  TextSize::uninstall();
  qunsetenv("DESKFLOW_APPEARANCE");
  Settings::setValue(Settings::Gui::TextSize);
  Settings::setValue(Settings::Gui::Appearance);
  QApplication::setFont(QFont());
}

void TextSizeTests::withoutTheService_theControlsAreAsTheyWereDrawn()
{
  QVERIFY(!TextSize::instance());
  QCOMPARE(metricScale(), 1.0);
  QCOMPARE(px(44), 44);
  QCOMPARE(px(1), 1);
}

void TextSizeTests::windows_raisesASmallSystemFont_andTheControlsStayAsDrawn()
{
  FakeSystem system;
  system.pixels = 12;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  QCOMPARE(TextSize::instance(), service);

  // the body of the settings of Windows 11 is 14 px: the font is made that, and the controls are what they were drawn
  QCOMPARE(appFontPixels(), 14);
  QVERIFY(service->plan().raisedByFloor);
  QCOMPARE(service->plan().bodyPixels, 14.0);
  QCOMPARE(metricScale(), 1.0);
  QCOMPARE(px(44), 44);
  // the font is the one of the system but for its size
  QCOMPARE(QApplication::font().family(), service->systemFont().family());
}

void TextSizeTests::mac_keepsTheFontOfTheSystemAsItIs_andTellsNoWidget()
{
  FakeSystem system;
  system.pixels = 13;
  system.platform = TextPlatform::Mac;
  QApplication::setFont(fontOfPixels(13));
  FontWatcher watcher;
  const auto before = QApplication::font();
  auto *service = TextSize::install(system.environment());
  QVERIFY(!service->plan().raisedByFloor);
  QCOMPARE(QApplication::font(), before);
  QCOMPARE(appFontPixels(), 13);
  QCOMPARE(metricScale(), 1.0);
  QCOMPARE(watcher.changes, 0);
}

void TextSizeTests::thePercentIsWritten_theFontAndTheControlsFollow_andTheWidgetsAreTold()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  FontWatcher watcher;
  QSignalSpy planChanged(service, &TextSize::planChanged);

  service->setPercent(150);
  // 150 % of the 14 px of the floor
  QCOMPARE(Settings::value(Settings::Gui::TextSize).toInt(), 150);
  QCOMPARE(service->percent(), 150);
  QCOMPARE(appFontPixels(), 21);
  QCOMPARE(metricScale(), 1.5);
  QCOMPARE(px(44), 66);
  QCOMPARE(px(46), 69);
  QVERIFY(watcher.changes >= 1);
  QVERIFY(planChanged.count() >= 1);
  QCOMPARE(planChanged.last().first().value<TextSizePlan>().bodyPixels, 21.0);

  // back to the default: the font is the floor again, and the controls as drawn
  watcher.changes = 0;
  service->setPercent(100);
  QCOMPARE(appFontPixels(), 14);
  QCOMPARE(metricScale(), 1.0);
  QVERIFY(watcher.changes >= 1);

  // the same percent again changes nothing and tells nobody
  watcher.changes = 0;
  planChanged.clear();
  service->setPercent(100);
  QCOMPARE(watcher.changes, 0);
  QCOMPARE(planChanged.count(), 0);
}

void TextSizeTests::aChangeOfTheSettingFromElsewhere_isFollowed()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  // the page of the settings and the keys write it, and the service follows
  Settings::setValue(Settings::Gui::TextSize, 130);
  QCOMPARE(service->percent(), 130);
  QCOMPARE(appFontPixels(), 18);
  Settings::setValue(Settings::Gui::TextSize, 90);
  QCOMPARE(service->percent(), 90);
  QCOMPARE(appFontPixels(), 13);
  QCOMPARE(metricScale(), 1.0);
}

void TextSizeTests::aStoredPercentThatIsNoStep_isTheDefault()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  Settings::setValue(Settings::Gui::TextSize, 95);
  auto *service = TextSize::install(system.environment());
  QCOMPARE(service->percent(), 100);
  QCOMPARE(appFontPixels(), 14);
  Settings::setValue(Settings::Gui::TextSize, 777);
  QCOMPARE(service->percent(), 100);
  QCOMPARE(appFontPixels(), 14);
}

void TextSizeTests::steps_goOneAtATime_andStopAtTheEnds_andResetGoesBackToTheDefault()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  service->step(1);
  QCOMPARE(service->percent(), 115);
  service->step(1);
  QCOMPARE(service->percent(), 130);
  service->step(1);
  QCOMPARE(service->percent(), 150);
  service->step(1);
  QCOMPARE(service->percent(), 150);
  QCOMPARE(Settings::value(Settings::Gui::TextSize).toInt(), 150);
  service->step(0);
  QCOMPARE(service->percent(), 100);
  service->step(-1);
  QCOMPARE(service->percent(), 90);
  service->step(-1);
  QCOMPARE(service->percent(), 90);
}

void TextSizeTests::refresh_readsTheSystemAgain()
{
  FakeSystem system;
  system.pixels = 12;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  QCOMPARE(appFontPixels(), 14);

  // the person chose a larger font in the settings of the OS and came back to the application
  system.pixels = 18;
  service->refresh();
  QCOMPARE(appFontPixels(), 18);
  QVERIFY(!service->plan().raisedByFloor);
  QCOMPARE(metricScale(), 18.0 / 14.0);

  // and a smaller one: the floor is back
  system.pixels = 11;
  service->refresh();
  QCOMPARE(appFontPixels(), 14);
  QCOMPARE(metricScale(), 1.0);
}

void TextSizeTests::theTextSizeOfTheOs_isMultipliedIntoTheAutomaticSize()
{
  FakeSystem system;
  system.pixels = 12;
  system.osScale = 1.5;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  // Windows: Settings > Accessibility > Text size 150 %
  QCOMPARE(appFontPixels(), 18);
  QVERIFY(!service->plan().raisedByFloor);
  system.osScale = 1.0;
  service->refresh();
  QCOMPARE(appFontPixels(), 14);
}

void TextSizeTests::aFontInPoints_keepsItsPointsAndItsFamily()
{
  // the system gives a size in points: the application has points too, and not a size in pixels
  TextSizeEnvironment env;
  QFont system = QApplication::font();
  system.setPointSizeF(9.0);
  env.systemFont = [system] { return system; };
  env.platform = TextPlatform::Windows;
  QApplication::setFont(system);
  const auto pixelsOf9Points = QFontInfo(system).pixelSize();
  auto *service = TextSize::install(env);
  QVERIFY(QApplication::font().pointSizeF() > 0);
  QVERIFY(QApplication::font().pixelSize() <= 0);
  // 9 points are below the floor wherever the point is 1.33 pixels or fewer: the body is 14 px in any case
  const auto expected = pixelsOf9Points <= 14 ? 14 : pixelsOf9Points;
  QVERIFY2(
      qAbs(QFontInfo(QApplication::font()).pixelSize() - expected) <= 1,
      qPrintable(QString::number(QFontInfo(QApplication::font()).pixelSize()))
  );
  QCOMPARE(QApplication::font().family(), system.family());
  Q_UNUSED(service);
}

void TextSizeTests::aWidgetThatSetsItsOwnFont_isToldOfTheNewSize_throughTheLookFollower()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());

  // a title: its font is its own (a ratio of the font of the application), so Qt does not tell it of a new one, and it
  // is a child, which is not told of the font of the application either
  QWidget window;
  auto *title = new QLabel(QStringLiteral("Title"), &window);
  new LookFollower(title, [title] { title->setFont(scaleFont(QApplication::font(title), 26.0 / 14.0)); });
  QCOMPARE(QFontInfo(title->font()).pixelSize(), 26);

  service->setPercent(150);
  // 21 px of body, and a title of 26 / 14 of it
  QCOMPARE(QFontInfo(title->font()).pixelSize(), 39);
  service->setPercent(90);
  // 12.6 px of body are 13 px of font (the system of the test gives pixels), and a title of 26 / 14 of that
  QCOMPARE(QFontInfo(title->font()).pixelSize(), 24);
  service->setPercent(100);
  QCOMPARE(QFontInfo(title->font()).pixelSize(), 26);
}

void TextSizeTests::onTextSizeChanged_endsWithItsContext()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  int calls = 0;
  {
    QObject context;
    onTextSizeChanged(&context, [&calls] { ++calls; });
    service->setPercent(130);
    QCOMPARE(calls, 1);
    // no change, no call
    service->setPercent(130);
    QCOMPARE(calls, 1);
  }
  service->setPercent(115);
  QCOMPARE(calls, 1);
}

void TextSizeTests::fontRatio_isHowManyTimesTheFontOfTheSystemTheTextIs()
{
  QCOMPARE(fontRatio(), 1.0);
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  // Windows: 14 px of text on a system of 12 px
  QCOMPARE(fontRatio(), 14.0 / 12.0);
  service->setPercent(150);
  QCOMPARE(fontRatio(), 21.0 / 12.0);
  service->setPercent(100);
  TextSize::uninstall();
  QCOMPARE(fontRatio(), 1.0);

  // a Mac that has the size of its settings has the font as it is
  system.pixels = 13;
  system.platform = TextPlatform::Mac;
  QApplication::setFont(fontOfPixels(13));
  TextSize::install(system.environment());
  QCOMPARE(fontRatio(), 1.0);
}

void TextSizeTests::describe_saysTheSizeAndWhereItComesFrom()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  auto *service = TextSize::install(system.environment());
  QCOMPARE(
      service->describe(),
      QStringLiteral(
          "text size: body 14.00 px (system font 12.00 px x OS text size 1.00, least 14 px, raised), step 100 %, "
          "controls x1.00"
      )
  );

  system.osScale = 1.5;
  service->setPercent(130);
  QCOMPARE(
      service->describe(),
      QStringLiteral(
          "text size: body 23.40 px (system font 12.00 px x OS text size 1.50, least 14 px), step 130 %, "
          "controls x1.67"
      )
  );
}

void TextSizeTests::uninstall_putsTheFontOfTheSystemBack()
{
  FakeSystem system;
  QApplication::setFont(fontOfPixels(12));
  TextSize::install(system.environment())->setPercent(150);
  QCOMPARE(appFontPixels(), 21);
  FontWatcher watcher;
  TextSize::uninstall();
  QVERIFY(!TextSize::instance());
  QCOMPARE(appFontPixels(), 12);
  QCOMPARE(metricScale(), 1.0);
  QCOMPARE(px(44), 44);
  QVERIFY(watcher.changes >= 1);
}

void TextSizeTests::applyAppearance_hasTheServiceForTheNewLook_andNotForTheSystem()
{
  const auto before = QApplication::font();

  // the look of the system: no service, and the font as the OS gave it
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("system"));
  applyAppearance();
  QVERIFY(!TextSize::instance());
  QCOMPARE(QApplication::font(), before);
  QCOMPARE(metricScale(), 1.0);

  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("deskflow"));
  applyAppearance();
  QVERIFY(TextSize::instance());

  // and back: the service ends, the font is the one of the system again
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("system"));
  applyAppearance();
  QVERIFY(!TextSize::instance());
  QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), QFontInfo(before).pixelSize());
  QCOMPARE(metricScale(), 1.0);
}

QTEST_MAIN(TextSizeTests)
