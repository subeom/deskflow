# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME TrayWindowTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TrayWindowTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the header of the main window includes the headers next to it by their short names
target_include_directories(TrayWindowTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
# the icons of the program (the tray menu asks for them by their names)
target_sources(TrayWindowTests PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/res/deskflow.qrc")

create_test(
  NAME TrayPolicyTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TrayPolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TrayPolicyTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TrayStateTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TrayStateTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TrayStateTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TrayMenuModelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TrayMenuModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TrayMenuModelTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TrayIconTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TrayIconTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TrayIconTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
# the mark of the program, which the icons are made of
target_sources(TrayIconTests PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/res/deskflow.qrc")

