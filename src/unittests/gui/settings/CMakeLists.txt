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

if(APPLE)
  create_test(
    NAME InputIndicatorSettingsTests
    DEPENDS gui
    LIBS server ${gui_server_libs}
    SOURCE InputIndicatorSettingsTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )
  target_include_directories(InputIndicatorSettingsTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
endif()

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

# The card of the option that turns off the wireless link of AirDrop while sharing (macOS), its banner, and their Korean
# (the .qm that is shipped: the test builds it first)
create_test(
  NAME AwdlPageTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE AwdlPageTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(AwdlPageTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
target_compile_definitions(AwdlPageTests PRIVATE DESKFLOW_KO_QM="${CMAKE_BINARY_DIR}/translations/deskflow_ko.qm")
add_dependencies(AwdlPageTests app_translations)
