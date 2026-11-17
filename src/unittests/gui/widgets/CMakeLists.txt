# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME ArrangementCanvasTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ArrangementCanvasTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ToggleSwitchTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ToggleSwitchTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME SettingWidgetsTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE SettingWidgetsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ShellWidgetsTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ShellWidgetsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME SettingAccessibilityTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE SettingAccessibilityTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LogDockTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE LogDockTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
