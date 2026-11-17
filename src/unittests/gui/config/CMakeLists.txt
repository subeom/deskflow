# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME ComputerTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ComputerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ArrangementConfigTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ArrangementConfigTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
