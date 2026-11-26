/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsKeyStateTests.h"

#include "../deskflow/MockEventQueue.h"
#include "deskflow/KeyMap.h"
#include "platform/MSWindowsKeyState.h"

#include <QTest>

#include <array>
#include <utility>
#include <vector>

namespace {

struct FakeMediaEvent
{
  WORD virtualKey = 0;
  DWORD flags = 0;
};

class RecordingMSWindowsKeyState : public MSWindowsKeyState
{
public:
  RecordingMSWindowsKeyState(IEventQueue *events, deskflow::KeyMap &keyMap)
      : MSWindowsKeyState(nullptr, nullptr, events, keyMap, {"en"}, true)
  {
  }

  const std::vector<FakeMediaEvent> &events() const
  {
    return m_events;
  }

  void clearEvents()
  {
    m_events.clear();
  }

protected:
  void sendSemanticMediaKeyEvent(WORD virtualKey, DWORD flags) override
  {
    m_events.push_back({virtualKey, flags});
  }

private:
  std::vector<FakeMediaEvent> m_events;
};

} // namespace

void MSWindowsKeyStateTests::semanticMedia_synthesizesSupportedVirtualKeysAsClicks()
{
  MockEventQueue eventQueue;
  deskflow::KeyMap keyMap;
  RecordingMSWindowsKeyState keyState(&eventQueue, keyMap);

  constexpr std::array mappings = {
      std::pair{kKeyAudioUp, VK_VOLUME_UP},          std::pair{kKeyAudioDown, VK_VOLUME_DOWN},
      std::pair{kKeyAudioMute, VK_VOLUME_MUTE},      std::pair{kKeyAudioPrev, VK_MEDIA_PREV_TRACK},
      std::pair{kKeyAudioNext, VK_MEDIA_NEXT_TRACK}, std::pair{kKeyAudioPlay, VK_MEDIA_PLAY_PAUSE},
      std::pair{kKeyAudioStop, VK_MEDIA_STOP},
  };

  for (const auto &[id, virtualKey] : mappings) {
    keyState.clearEvents();
    QVERIFY(keyState.fakeSemanticMediaKey(id));
    QCOMPARE(keyState.events().size(), static_cast<size_t>(2));
    QCOMPARE(keyState.events()[0].virtualKey, virtualKey);
    QCOMPARE(keyState.events()[0].flags, static_cast<DWORD>(KEYEVENTF_EXTENDEDKEY));
    QCOMPARE(keyState.events()[1].virtualKey, virtualKey);
    QCOMPARE(keyState.events()[1].flags, static_cast<DWORD>(KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP));
  }
}

void MSWindowsKeyStateTests::semanticMedia_rejectsUnsupportedKeys()
{
  MockEventQueue eventQueue;
  deskflow::KeyMap keyMap;
  RecordingMSWindowsKeyState keyState(&eventQueue, keyMap);

  constexpr std::array unsupported = {kKeyBrightnessUp, kKeyBrightnessDown, kKeyMissionControl,
                                      kKeyLaunchpad,    kKeyEject,          kKeyF1,
                                      kKeyF20};
  for (const auto id : unsupported) {
    QVERIFY(!keyState.fakeSemanticMediaKey(id));
  }
  QVERIFY(keyState.events().empty());
}

QTEST_MAIN(MSWindowsKeyStateTests)
