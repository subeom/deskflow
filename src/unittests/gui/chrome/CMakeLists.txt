# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME ChromePolicyTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ChromePolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(ChromePolicyTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME ChromeGeometryTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ChromeGeometryTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(ChromeGeometryTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME MenuMigrationTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE MenuMigrationTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(MenuMigrationTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME WindowChromeTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE WindowChromeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(WindowChromeTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
# the icons of the program (the header asks for them by their names)
target_sources(WindowChromeTests PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/res/deskflow.qrc")

# the rules of the native event filter of Windows, run with a made-up window; the scan of the sources reads the files of the
# gui library
create_test(
  NAME FrameMessagesTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE FrameMessagesTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(FrameMessagesTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
target_compile_definitions(FrameMessagesTests PRIVATE DESKFLOW_GUI_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/lib/gui")
