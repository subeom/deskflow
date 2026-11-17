# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME ArrangementCanvasTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ArrangementCanvasTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
