# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME StatusModelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE StatusModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MainShellTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE MainShellTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the header of the main window includes the headers next to it by their short names
target_include_directories(MainShellTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TextSizeShellTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TextSizeShellTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TextSizeShellTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
