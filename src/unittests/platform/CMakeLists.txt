# SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

if (WIN32)
  create_test(
    NAME MSWindowsClipboardTests
    DEPENDS platform
    LIBS base arch
    SOURCE MSWindowsClipboardTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  # A key state of Windows is a KeyState and has a KeyMap, and its computer is a PlatformComputer: those, and the threads
  # that they use, are in the libraries of the program. The platform library links the app library on Unix only (see
  # src/lib/platform/CMakeLists.txt), so here the test names what the tests of the server name.
  create_test(
    NAME MSWindowsKeyStateTests
    DEPENDS platform
    LIBS base arch io mt net app version
    SOURCE MSWindowsKeyStateTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
elseif(APPLE)
  create_test(
    NAME OSXClipboardTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXClipboardTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME OSXDisplaysTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXDisplaysTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME OSXKeyGroupResolverTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXKeyGroupResolverTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME OSXKeyStateTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXKeyStateTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME OSXInputSourceTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXInputSourceTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  create_test(
    NAME OSXInputIndicatorRelayTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXInputIndicatorRelayTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  # the hold of the pointer while it is on a client: a policy, a line of the log and a controller, with fakes (no event, no
  # cursor, no event tap)
  create_test(
    NAME OSXPointerHoldTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXPointerHoldTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  # the real observers of the notification center of NSWorkspace of the test process, with synthetic notifications that
  # are posted to that center only (the distributed center is system wide and is never posted to)
  create_test(
    NAME OSXPointerHoldNotificationsTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXPointerHoldNotificationsTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
  target_sources(OSXPointerHoldNotificationsTests PRIVATE OSXPointerHoldTestSupport.h OSXPointerHoldTestSupport.mm)
  create_test(
    NAME OSXScrollSourceTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXScrollSourceTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
elseif(UNIX)
  if(LIBEI_FOUND)
    create_test(
      NAME EiHotKeyModifierMatchingTests
      DEPENDS platform
      LIBS base arch
      SOURCE EiHotKeyModifierMatchingTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )

    create_test(
      NAME EiKeyStateTests
      DEPENDS platform
      LIBS base arch
      SOURCE EiKeyStateTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )
  endif()

  if (BUILD_X11_SUPPORT)
    create_test(
      NAME XWindowsClipboardTests
      DEPENDS platform
      LIBS base arch
      SOURCE XWindowsClipboardTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )
  endif()
endif()
