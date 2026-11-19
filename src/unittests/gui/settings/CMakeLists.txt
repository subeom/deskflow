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

# the page of the client has the group of the Cloudflare Tunnel (with a store in memory): it links the libraries of the
# server like the other tests of widgets
create_test(
  NAME ClientPageTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ClientPageTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(ClientPageTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
