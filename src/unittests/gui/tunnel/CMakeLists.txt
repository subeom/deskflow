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

create_test(
  NAME TunnelStartGuardTests
  DEPENDS gui
  SOURCE TunnelStartGuardTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME SecretStoreTests
  DEPENDS gui
  SOURCE SecretStoreTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# These two open the dialog and the group of the Cloudflare Tunnel, which the gui library builds with the server library
# like the other tests of dialogs (see ../dialogs for why the Windows libraries are named here)
create_test(
  NAME TunnelSettingsGroupTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TunnelSettingsGroupTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ClientConfigDialogTunnelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE ClientConfigDialogTunnelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The process, the ports and the leftover processes of the tunnel. They start the stand-in of cloudflared (./fake), never
# the real one, and listen on ports that the system hands out.
add_subdirectory(fake)

create_test(
  NAME OutputLineSplitterTests
  DEPENDS gui
  SOURCE OutputLineSplitterTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

foreach(tunnel_io_test PortProberTests OrphanRecordTests QtTunnelProcessTests TunnelSupervisorProcessTests)
  create_test(
    NAME ${tunnel_io_test}
    DEPENDS gui
    SOURCE ${tunnel_io_test}.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )
  add_dependencies(${tunnel_io_test} cloudflared-fake)
  target_compile_definitions(${tunnel_io_test} PRIVATE FAKE_CLOUDFLARED_PATH="$<TARGET_FILE:cloudflared-fake>")
endforeach()

# The supervisor is run with fakes of everything it touches
create_test(
  NAME TunnelSupervisorTests
  DEPENDS gui
  SOURCE TunnelSupervisorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The controller reads and writes the settings (of a file in a scratch directory) and runs a supervisor made of fakes
create_test(
  NAME TunnelControllerTests
  DEPENDS gui
  SOURCE TunnelControllerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The two lines of the main window for the tunnel (a widget, so the gui library with the libraries of the server like the
# tests of the other widgets)
create_test(
  NAME TunnelStatusWidgetTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TunnelStatusWidgetTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The test of the whole way through the tunnel: the controller with a supervisor of fakes, and the connection of the test
# made by the test itself (the one test that uses sockets does so on ports that the system hands out)
create_test(
  NAME TunnelTestRunnerTests
  DEPENDS gui
  SOURCE TunnelTestRunnerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The dialog of the test (a dialog, so the gui library with the libraries of the server like the other dialog tests)
create_test(
  NAME TunnelTestDialogTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TunnelTestDialogTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The log lines of cloudflared that repeat while a tunnel is down are shown once in a while, with their count
create_test(
  NAME RepeatCollapserTests
  DEPENDS gui
  SOURCE RepeatCollapserTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The manual check of the secure store of the system (the keychain, the Credential Manager). Off: no test may call them.
option(DESKFLOW_BUILD_SECRET_SMOKE "Build secret-store-smoke, the manual check of the secure store of the system" OFF)
if(DESKFLOW_BUILD_SECRET_SMOKE)
  add_subdirectory(smoke)
endif()
