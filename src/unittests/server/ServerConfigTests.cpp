/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigTests.h"

#include "common/Settings.h"
#include "deskflow/CrossPlatformInputPolicy.h"
#include "server/Config.h"

#include <array>
#include <cmath>
#include <optional>
#include <sstream>

class OnlySystemFilter : public InputFilter::Condition
{
public:
  Condition *clone() const override
  {
    return new OnlySystemFilter();
  }
  std::string format() const override
  {
    return "";
  }

  InputFilter::FilterStatus match(const Event &ev) override
  {
    return ev.getType() == EventTypes::System ? InputFilter::FilterStatus::Activate
                                              : InputFilter::FilterStatus::NoMatch;
  }
};

using namespace deskflow::server;

void ServerConfigTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  m_settingsFile = m_settingsDir.filePath(QStringLiteral("Deskflow.conf"));
  Settings::setSettingsFile(m_settingsFile);
}

void ServerConfigTests::f18Action_hangulAddsOption()
{
  const auto key = Settings::Computer::F18Action.arg(QStringLiteral("computerA"));
  Settings::setValue(key, QStringLiteral("hangul"));

  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  const auto *options = config.getOptions("computerA");
  QVERIFY(options != nullptr);
  auto option = options->find(kOptionCrossOSF18Key);
  QVERIFY(option != options->end());
  QCOMPARE(option->second, static_cast<OptionValue>(F18Action::Hangul));

  // parsed like the other text settings: trimmed and case-insensitive
  Settings::setValue(key, QStringLiteral(" HANGUL "));
  Config padded(nullptr);
  QVERIFY(padded.addComputer("computerA"));
  QCOMPARE(padded.getOptions("computerA")->count(kOptionCrossOSF18Key), static_cast<size_t>(1));
}

void ServerConfigTests::f18Action_passThroughMissingAndInvalidAddNoOption()
{
  const auto name = QStringLiteral("computerB");
  const auto key = Settings::Computer::F18Action.arg(name);
  const auto f18Options = [&]() {
    Config config(nullptr);
    if (!config.addComputer(name.toStdString())) {
      return static_cast<size_t>(-1);
    }
    const auto *options = config.getOptions(name.toStdString());
    return options == nullptr ? static_cast<size_t>(-1) : options->count(kOptionCrossOSF18Key);
  };

  // pass through is the default, so it is what an old config without the key gets: no option is sent at all
  Settings::setValue(key, QVariant());
  QCOMPARE(f18Options(), static_cast<size_t>(0));
  Settings::setValue(key, QStringLiteral("passthrough"));
  QCOMPARE(f18Options(), static_cast<size_t>(0));
  Settings::setValue(key, QStringLiteral("invalid"));
  QCOMPARE(f18Options(), static_cast<size_t>(0));
  Settings::setValue(key, QString());
  QCOMPARE(f18Options(), static_cast<size_t>(0));
}

void ServerConfigTests::legacyRightCommandAction_isIgnored()
{
  // a config written while Right Command could be mapped to Hangul still loads, and Command stays the Windows key:
  // neither the old value nor anything derived from it reaches the options
  const auto name = QStringLiteral("computerC");
  Settings::setValue(QStringLiteral("computer_%1/rightCommandAction").arg(name), QStringLiteral("hangul"));
  Settings::setValue(Settings::Computer::F18Action.arg(name), QVariant());

  Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  const auto *options = config.getOptions(name.toStdString());
  QVERIFY(options != nullptr);
  QCOMPARE(options->count(kOptionCrossOSF18Key), static_cast<size_t>(0));
  // "RCPM", the option ID the setting used to produce
  constexpr OptionID legacyRightCommandOption = ('R' << 24) | ('C' << 16) | ('P' << 8) | 'M';
  QCOMPARE(options->count(legacyRightCommandOption), static_cast<size_t>(0));

  // the other Cross-OS settings of the same computer are read as before
  Settings::setValue(Settings::Computer::CrossOSSemanticMedia.arg(name), true);
  Config withMedia(nullptr);
  QVERIFY(withMedia.addComputer(name.toStdString()));
  QCOMPARE(withMedia.getOptions(name.toStdString())->count(kOptionCrossOSSemanticMedia), static_cast<size_t>(1));
}

void ServerConfigTests::crossOSScrollOptions_areOptIn()
{
  const auto xKey = Settings::Computer::CrossOSInvertX.arg(QStringLiteral("scrollComputer"));
  const auto yKey = Settings::Computer::CrossOSInvertY.arg(QStringLiteral("scrollComputer"));
  Settings::setValue(xKey, true);
  Settings::setValue(yKey, false);

  Config xOnly(nullptr);
  QVERIFY(xOnly.addComputer("scrollComputer"));
  const auto *xOptions = xOnly.getOptions("scrollComputer");
  QVERIFY(xOptions != nullptr);
  auto option = xOptions->find(kOptionCrossOSInvertScrollX);
  QVERIFY(option != xOptions->end());
  QCOMPARE(option->second, 1);
  QCOMPARE(xOptions->count(kOptionCrossOSInvertScrollY), static_cast<size_t>(0));

  Settings::setValue(xKey, false);
  Settings::setValue(yKey, true);
  Config yOnly(nullptr);
  QVERIFY(yOnly.addComputer("scrollComputer"));
  const auto *yOptions = yOnly.getOptions("scrollComputer");
  QVERIFY(yOptions != nullptr);
  QCOMPARE(yOptions->count(kOptionCrossOSInvertScrollX), static_cast<size_t>(0));
  option = yOptions->find(kOptionCrossOSInvertScrollY);
  QVERIFY(option != yOptions->end());
  QCOMPARE(option->second, 1);
}

void ServerConfigTests::crossOSSemanticMediaOption_isOptIn()
{
  const auto key = Settings::Computer::CrossOSSemanticMedia.arg(QStringLiteral("mediaComputer"));
  Settings::setValue(key, true);

  Config enabled(nullptr);
  QVERIFY(enabled.addComputer("mediaComputer"));
  const auto *enabledOptions = enabled.getOptions("mediaComputer");
  QVERIFY(enabledOptions != nullptr);
  const auto enabledOption = enabledOptions->find(kOptionCrossOSSemanticMedia);
  QVERIFY(enabledOption != enabledOptions->end());
  QCOMPARE(enabledOption->second, 1);

  Settings::setValue(key, false);
  Config disabled(nullptr);
  QVERIFY(disabled.addComputer("mediaComputer"));
  const auto *disabledOptions = disabled.getOptions("mediaComputer");
  QVERIFY(disabledOptions != nullptr);
  QCOMPARE(disabledOptions->count(kOptionCrossOSSemanticMedia), static_cast<size_t>(0));
}

void ServerConfigTests::crossOSInvertScope_isOptIn()
{
  const auto name = QStringLiteral("scopeComputer");
  const auto key = Settings::Computer::CrossOSInvertScope.arg(name);
  const auto scopeOption = [&]() {
    Config config(nullptr);
    if (!config.addComputer(name.toStdString())) {
      return std::optional<OptionValue>();
    }
    const auto *options = config.getOptions(name.toStdString());
    if (options == nullptr || !options->contains(kOptionCrossOSInvertScope)) {
      return std::optional<OptionValue>();
    }
    return std::optional<OptionValue>(options->at(kOptionCrossOSInvertScope));
  };

  // an old config without the key, the default, and anything unrecognized mean "all": no option at all
  Settings::setValue(key, QVariant());
  QVERIFY(!scopeOption().has_value());
  Settings::setValue(key, QStringLiteral("all"));
  QVERIFY(!scopeOption().has_value());
  Settings::setValue(key, QStringLiteral("trackpadOnly"));
  QVERIFY(!scopeOption().has_value());
  Settings::setValue(key, QString());
  QVERIFY(!scopeOption().has_value());

  Settings::setValue(key, QStringLiteral("mouseWheelOnly"));
  QCOMPARE(scopeOption().value_or(-1), static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly));

  // parsed like f18Action: trimmed and case-insensitive
  Settings::setValue(key, QStringLiteral(" MouseWheelONLY "));
  QCOMPARE(scopeOption().value_or(-1), static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly));
}

namespace {

// The global options a server config reads from the settings
Config::ScreenOptions readGlobalOptions()
{
  Config config(nullptr);
  std::istringstream input("section: options\nend\n");
  input >> config;
  const auto *options = config.getOptions("");
  return options == nullptr ? Config::ScreenOptions() : *options;
}

} // namespace

void ServerConfigTests::clipboardSizeOption_data()
{
  QTest::addColumn<bool>("textOnly");
  QTest::addColumn<uint>("configuredMiB");
  QTest::addColumn<int>("expectedKiB");

  // CLSZ is in KiB on the wire, the setting is in MiB: off sends what it always sent
  QTest::newRow("off, 0 MiB") << false << 0u << 0;
  QTest::newRow("off, 1 MiB") << false << 1u << 1024;
  QTest::newRow("off, 3 MiB") << false << 3u << 3072;
  QTest::newRow("off, 20 MiB") << false << 20u << 20480;
  // text only is always 10 KiB, whatever the size box holds: it is greyed out then, and a 0 in it must not keep sharing
  // off (off with 0 is still off, as above)
  QTest::newRow("text only, 0 MiB") << true << 0u << 10;
  QTest::newRow("text only, 1 MiB") << true << 1u << 10;
  QTest::newRow("text only, 3 MiB") << true << 3u << 10;
  QTest::newRow("text only, 20 MiB") << true << 20u << 10;
}

// Peers learn of text-only mode through the size option they already honour, so no peer has to be rebuilt
void ServerConfigTests::clipboardSizeOption()
{
  QFETCH(bool, textOnly);
  QFETCH(uint, configuredMiB);
  QFETCH(int, expectedKiB);

  Settings::setValue(Settings::Server::EnableClipboard, true);
  Settings::setValue(Settings::Server::ClipboardSize, configuredMiB);
  Settings::setValue(Settings::Server::ClipboardTextOnly, false);
  const auto offOptions = readGlobalOptions();
  QVERIFY(offOptions.contains(kOptionClipboardSharingSize));
  QCOMPARE(offOptions.at(kOptionClipboardSharingSize), static_cast<int>(configuredMiB * 1024));

  Settings::setValue(Settings::Server::ClipboardTextOnly, textOnly);
  const auto options = readGlobalOptions();
  QVERIFY(options.contains(kOptionClipboardSharingSize));
  QCOMPARE(options.at(kOptionClipboardSharingSize), expectedKiB);

  // text only adds no option and changes no other: the size is the only thing a peer is told
  auto others = options;
  auto offOthers = offOptions;
  others.erase(kOptionClipboardSharingSize);
  offOthers.erase(kOptionClipboardSharingSize);
  QVERIFY(others == offOthers);
  QCOMPARE(options.at(kOptionClipboardSharing), 1);

  Settings::setValue(Settings::Server::ClipboardSize);
  Settings::setValue(Settings::Server::ClipboardTextOnly);
  Settings::setValue(Settings::Server::EnableClipboard);
}

void ServerConfigTests::clipboardSizeOption_oldConfigWithoutTheKeyIsOff()
{
  // a config from before the setting existed has no value for it
  Settings::setValue(Settings::Server::ClipboardSize);
  Settings::setValue(Settings::Server::ClipboardTextOnly);

  const auto options = readGlobalOptions();
  QCOMPARE(options.at(kOptionClipboardSharingSize), 3 * 1024);
}

void ServerConfigTests::monitorLayout_isOffUnlessTheComputerHasOne()
{
  const auto name = QStringLiteral("monitorLayoutOff");
  const auto key = Settings::Computer::MonitorLayout.arg(name);
  Settings::setValue(key);

  Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  QVERIFY(config.getMonitorLayout(name.toStdString()) == nullptr);

  // a layout of nothing is none
  Settings::setValue(key, QStringLiteral(" \n\t "));
  Config blank(nullptr);
  QVERIFY(blank.addComputer(name.toStdString()));
  QVERIFY(blank.getMonitorLayout(name.toStdString()) == nullptr);
  Settings::setValue(key);
}

void ServerConfigTests::monitorLayout_isReadFromTheSettings()
{
  const auto name = QStringLiteral("monitorLayoutRead");
  const auto key = Settings::Computer::MonitorLayout.arg(name);

  Settings::setValue(key, QStringLiteral(" 0,0,2560,1440 ; 2560,0,1920,1080\n"));
  Config config(nullptr);
  QVERIFY(config.addComputer(name.toStdString()));
  const auto *layout = config.getMonitorLayout(name.toStdString());
  QVERIFY(layout != nullptr);
  QVERIFY(layout->parsed);
  QCOMPARE(layout->parsed.layout->monitors().size(), size_t{2});
  QCOMPARE(layout->text, std::string("0,0,2560,1440 ; 2560,0,1920,1080"));

  // a layout that is wrong is kept, with what is wrong with it, so that it can be said when it matters
  Settings::setValue(key, QStringLiteral("0,0,2560"));
  Config wrong(nullptr);
  QVERIFY(wrong.addComputer(name.toStdString()));
  layout = wrong.getMonitorLayout(name.toStdString());
  QVERIFY(layout != nullptr);
  QVERIFY(!layout->parsed);
  QVERIFY(!layout->parsed.error.empty());

  Settings::setValue(key);
}

void ServerConfigTests::monitorLayout_addsNoOption()
{
  // an option goes to the computer, which is told the options of its own: a layout is not for it to know
  const auto name = QStringLiteral("monitorLayoutOptions");
  const auto key = Settings::Computer::MonitorLayout.arg(name);
  Settings::setValue(key);
  Config without(nullptr);
  QVERIFY(without.addComputer(name.toStdString()));

  Settings::setValue(key, QStringLiteral("0,0,2560,1440;2560,0,1920,1080"));
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Config with(nullptr);
  QVERIFY(with.addComputer(name.toStdString()));

  QVERIFY(*with.getOptions(name.toStdString()) == *without.getOptions(name.toStdString()));
  // nor does the master switch add one that a computer is told
  const auto onOptions = readGlobalOptions();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, false);
  QVERIFY(onOptions == readGlobalOptions());

  Settings::setValue(key);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void ServerConfigTests::monitorLayout_isPartOfEquality()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("pc"));
  QVERIFY(b.addComputer("pc"));
  QVERIFY(a == b);

  QVERIFY(b.setMonitorLayout("pc", "0,0,2560,1440;2560,0,1920,1080"));
  QVERIFY(!(a == b));
  QVERIFY(a.setMonitorLayout("pc", "0,0,2560,1440;2560,0,1920,1080"));
  QVERIFY(a == b);

  // an empty layout takes it away
  QVERIFY(a.setMonitorLayout("pc", ""));
  QVERIFY(a.getMonitorLayout("pc") == nullptr);
  QVERIFY(!a.setMonitorLayout("nobody", "0,0,10,10"));
}

void ServerConfigTests::neighborPositions_stayHalfOpenAndChain()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("source"));
  QVERIFY(config.addComputer("rightBridge"));
  QVERIFY(config.addComputer("rightFinal"));
  QVERIFY(config.addComputer("bottomBridge"));
  QVERIFY(config.addComputer("bottomFinal"));
  QVERIFY(config.connect("source", Direction::Right, 0.0f, 1.0f, "rightBridge", 0.5f, 1.0f));
  QVERIFY(config.connect("source", Direction::Bottom, 0.0f, 1.0f, "bottomBridge", 0.5f, 1.0f));
  QVERIFY(config.connect("rightBridge", Direction::Right, 0.0f, 1.0f, "rightFinal", 0.0f, 1.0f));
  QVERIFY(config.connect("bottomBridge", Direction::Bottom, 0.0f, 1.0f, "bottomFinal", 0.0f, 1.0f));

  struct Route
  {
    Direction direction;
    const char *bridge;
    const char *final;
  };
  constexpr std::array routes = {
      Route{Direction::Right, "rightBridge", "rightFinal"},
      Route{Direction::Bottom, "bottomBridge", "bottomFinal"},
  };
  const auto upperEdge = std::nextafter(1.0f, 0.0f);

  for (const auto &route : routes) {
    float positionOut = 0.0f;
    QCOMPARE(config.getNeighbor("source", route.direction, 0.25f, &positionOut), std::string(route.bridge));
    QCOMPARE(positionOut, 0.625f);

    QCOMPARE(config.getNeighbor("source", route.direction, upperEdge, &positionOut), std::string(route.bridge));
    QVERIFY(positionOut < 1.0f);
    QCOMPARE(positionOut, upperEdge);

    float finalPositionOut = 0.0f;
    QCOMPARE(
        config.getNeighbor(route.bridge, route.direction, positionOut, &finalPositionOut), std::string(route.final)
    );
    QVERIFY(finalPositionOut < 1.0f);
  }
}

void ServerConfigTests::partialDestinationInterval_clampsInsideEndpoint()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("source"));
  QVERIFY(config.addComputer("bridge"));
  QVERIFY(config.addComputer("final"));
  QVERIFY(config.connect("source", Direction::Right, 0.0f, 1.0f, "bridge", 0.01f, 0.04f));
  QVERIFY(config.connect("bridge", Direction::Right, 0.01f, 0.04f, "final", 0.0f, 1.0f));

  float normalPosition = 0.0f;
  QCOMPARE(config.getNeighbor("source", Direction::Right, 0.5f, &normalPosition), std::string("bridge"));
  QCOMPARE(normalPosition, 0.01f + (0.04f - 0.01f) * 0.5f);

  float edgePosition = 0.0f;
  QCOMPARE(
      config.getNeighbor("source", Direction::Right, std::nextafter(1.0f, 0.0f), &edgePosition), std::string("bridge")
  );
  QVERIFY(edgePosition >= 0.01f);
  QVERIFY(edgePosition < 0.04f);
  QCOMPARE(edgePosition, std::nextafter(0.04f, 0.01f));

  float finalPosition = 0.0f;
  QCOMPARE(config.getNeighbor("bridge", Direction::Right, edgePosition, &finalPosition), std::string("final"));
  QVERIFY(finalPosition < 1.0f);
}

void ServerConfigTests::equalityCheck()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a != b);

  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);

  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a.addOption(std::string(), kOptionClipboardSharing, 1));
  QVERIFY(b.addOption(std::string(), kOptionClipboardSharing, 1));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  b.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  /* TODO Fix linking to the proper libs
  NetworkAddress addr1("localhost", 8080);
  addr1.resolve();
  NetworkAddress addr2("localhost", 8080);
  addr2.resolve();
  a.setDeskflowAddress(addr1);
  b.setDeskflowAddress(addr2);
  */
  QVERIFY(a == b);
}

void ServerConfigTests::equalityCheck_diff_options()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 0));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_alias()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  QVERIFY(a != b);

  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasB"));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_filters()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a != b);
}

// TODO FIX
/*
void ServerConfigTests::equalityCheck_diff_address()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  a.setDeskflowAddress(NetworkAddress(8000));
  b.setDeskflowAddress(NetworkAddress(9000));
  QVERIFY(a != b);
}
*/

void ServerConfigTests::equalityCheck_diff_neighbours1()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);
  QVERIFY(b != a);
}

void ServerConfigTests::equalityCheck_diff_neighbours2()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.25f, "computerB", 0.25f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_neighbours3()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerC", 0.5f, 1.0f));
  QVERIFY(a != b);
}

QTEST_MAIN(ServerConfigTests)
