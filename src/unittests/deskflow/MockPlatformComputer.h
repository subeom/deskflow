/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IPlatformComputer.h"

//! Inert primary platform computer
/*!
Reports a fixed 1728x1117 screen with the cursor in its middle and does nothing else, so a
real deskflow::Computer, PrimaryClient and Server can be built without touching the OS.
*/
class MockPlatformComputer : public IPlatformComputer
{
public:
  explicit MockPlatformComputer(const IEventQueue *events) : IPlatformComputer(events)
  {
    // do nothing
  }

  void *getEventTarget() const override
  {
    return const_cast<MockPlatformComputer *>(this);
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = 0;
    y = 0;
    w = 1728;
    h = 1117;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 864;
    y = 558;
  }
  void reconfigure(uint32_t) override
  {
  }
  uint32_t activeSides() override
  {
    return 0;
  }
  void warpCursor(int32_t, int32_t) override
  {
  }
  uint32_t registerHotKey(KeyID, KeyModifierMask) override
  {
    return 1;
  }
  void unregisterHotKey(uint32_t) override
  {
  }
  void fakeInputBegin() override
  {
  }
  void fakeInputEnd() override
  {
  }
  int32_t getJumpZoneSize() const override
  {
    return 1;
  }
  bool isAnyMouseButtonDown(uint32_t &) const override
  {
    return false;
  }
  void getCursorCenter(int32_t &x, int32_t &y) const override
  {
    x = 864;
    y = 558;
  }
  void fakeMouseButton(ButtonID, bool) override
  {
  }
  void fakeMouseMove(int32_t, int32_t) override
  {
  }
  void fakeMouseRelativeMove(int32_t, int32_t) const override
  {
  }
  void fakeMouseWheel(ScrollDelta) const override
  {
  }
  void updateKeyMap() override
  {
  }
  void updateKeyState() override
  {
  }
  void setHalfDuplexMask(KeyModifierMask) override
  {
  }
  void fakeKeyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  bool fakeKeyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
    return true;
  }
  bool fakeKeyUp(KeyButton) override
  {
    return true;
  }
  void fakeAllKeysUp() override
  {
  }
  bool fakeCtrlAltDel() override
  {
    return true;
  }
  bool isKeyDown(KeyButton) const override
  {
    return false;
  }
  KeyModifierMask getActiveModifiers() const override
  {
    return 0;
  }
  KeyModifierMask pollActiveModifiers() const override
  {
    return 0;
  }
  int32_t pollActiveGroup() const override
  {
    return 0;
  }
  void pollPressedKeys(KeyButtonSet &) const override
  {
  }
  void enable() override
  {
  }
  void disable() override
  {
  }
  void enter() override
  {
  }
  bool canLeave() override
  {
    return true;
  }
  void leave() override
  {
  }
  bool setClipboard(ClipboardID, const IClipboard *) override
  {
    return true;
  }
  void checkClipboards() override
  {
  }
  void openScreensaver(bool) override
  {
  }
  void closeScreensaver() override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
  }
  void setSequenceNumber(uint32_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }
  bool isPrimary() const override
  {
    return true;
  }

protected:
  void handleSystemEvent(const Event &) override
  {
  }
};
