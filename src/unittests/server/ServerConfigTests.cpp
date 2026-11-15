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

void ServerConfigTests::rightCommandAction_hangulAddsOption()
{
  const auto key = Settings::Computer::RightCommandAction.arg(QStringLiteral("computerA"));
  Settings::setValue(key, QStringLiteral("hangul"));

  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  const auto *options = config.getOptions("computerA");
  QVERIFY(options != nullptr);
  auto option = options->find(kOptionCrossOSRightCommand);
  QVERIFY(option != options->end());
  QCOMPARE(option->second, static_cast<OptionValue>(RightCommandAction::Hangul));

  Settings::setValue(key, QStringLiteral("invalid"));
  Config fallback(nullptr);
  QVERIFY(fallback.addComputer("computerA"));
  const auto *fallbackOptions = fallback.getOptions("computerA");
  QVERIFY(fallbackOptions != nullptr);
  option = fallbackOptions->find(kOptionCrossOSRightCommand);
  QVERIFY(option == fallbackOptions->end());
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
