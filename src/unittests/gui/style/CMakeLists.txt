# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME DesignTokensTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE DesignTokensTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
