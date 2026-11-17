# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME DesignTokensTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE DesignTokensTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME AppearanceHintsTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE AppearanceHintsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME AppearanceTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE AppearanceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME BackdropPanelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE BackdropPanelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME IconsTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE IconsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME DeskflowStyleTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE DeskflowStyleTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
