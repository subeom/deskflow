# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME DisplaySignatureTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE DisplaySignatureTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LayoutStoreTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE LayoutStoreTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LayoutMatcherTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE LayoutMatcherTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LayoutMemoryTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE LayoutMemoryTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LayoutReattachTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE LayoutReattachTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
