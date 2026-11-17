# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME SettingsPagesTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE SettingsPagesTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the header of the dialog of the settings includes the headers next to it by their short names
target_include_directories(SettingsPagesTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
