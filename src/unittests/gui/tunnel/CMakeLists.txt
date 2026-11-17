# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# The logic of the Cloudflare Tunnel is plain Qt and the common library: these tests link the gui library only. A test
# that includes a dialog or a widget of the gui library needs the libraries of the server too (see ../dialogs).

create_test(
  NAME TunnelStateMachineTests
  DEPENDS gui
  SOURCE TunnelStateMachineTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME CloudflaredLocatorTests
  DEPENDS gui
  SOURCE CloudflaredLocatorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME CloudflaredLaunchTests
  DEPENDS gui
  SOURCE CloudflaredLaunchTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME CloudflaredLogParserTests
  DEPENDS gui
  SOURCE CloudflaredLogParserTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME SecretRedactorTests
  DEPENDS gui
  SOURCE SecretRedactorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
