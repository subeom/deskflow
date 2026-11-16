# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME DialogLayoutTests
  DEPENDS gui
  LIBS server
  SOURCE DialogLayoutTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ServerConfigDialogTests
  DEPENDS gui
  LIBS server
  SOURCE ServerConfigDialogTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MonitorEditorDialogTests
  DEPENDS gui
  LIBS server
  SOURCE MonitorEditorDialogTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the header of the server configuration dialog includes the headers next to it by their short names
foreach(test DialogLayoutTests ServerConfigDialogTests MonitorEditorDialogTests)
  target_include_directories(${test} PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
endforeach()
