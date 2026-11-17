# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME NetworkMonitorTests
  DEPENDS gui
  SOURCE NetworkMonitorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME AddressClassifierTests
  DEPENDS gui
  SOURCE AddressClassifierTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ConnectionProbeTests
  DEPENDS gui
  SOURCE ConnectionProbeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The gate of the Cloudflare Tunnel in the process of the core (the gui library, with the libraries of the server like the
# other tests that build the configuration of the server)
create_test(
  NAME CoreProcessTunnelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE CoreProcessTunnelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The stop of the service core at the quit of the program (a stand-in of the client of the service: no daemon, no socket)
create_test(
  NAME ServiceQuitTests
  DEPENDS gui
  SOURCE ServiceQuitTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
