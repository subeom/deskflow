# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

# The tests that link the server library (the configuration of the server, the arrangement, the monitor editor) reach
# objects of it that need the library of the program (KeyMap, IKeyState, ClipboardSharePolicy). The server library links
# that library on Unix only, so on Windows these tests name what the tests of the server name (see ../server).
if(WIN32)
  set(gui_server_libs base arch version app mt net)
endif()

add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(dialogs)
add_subdirectory(layout)
add_subdirectory(settings)
add_subdirectory(shell)
add_subdirectory(style)
add_subdirectory(tray)
add_subdirectory(widgets)

create_test(
  NAME LoggerTests
  DEPENDS gui
  SOURCE LoggerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
