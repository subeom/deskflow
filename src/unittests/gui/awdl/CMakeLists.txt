# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# The option that turns off the wireless link of AirDrop while sharing (macOS): its rules are plain Qt, so the tests link
# the gui library only. No test runs a program of the system or shows a dialog: the system is a fake (IAwdlSystem).

create_test(
  NAME AwdlModelTests
  DEPENDS gui
  SOURCE AwdlModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME AwdlControllerTests
  DEPENDS gui
  SOURCE AwdlControllerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MacAwdlSystemTests
  DEPENDS gui
  SOURCE MacAwdlSystemTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME AwdlPermissionTests
  DEPENDS gui
  SOURCE AwdlPermissionTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
