# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  set(extra_libs version)
endif()

create_test(
  NAME ServerProxyTests
  DEPENDS client
  LIBS arch base io mt net platform server app ${extra_libs}
  SOURCE ServerProxyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/client"
)

# On Windows this test is skipped (by the test itself, in initTestCase) when a Deskflow is running on the computer: an
# application object of Windows opens the machine-wide event Global\DeskflowClose that the installed Deskflow (its
# service, the session 0 daemon) owns, and the process stops if it cannot. Stop Deskflow to run it. The test never signals
# that event: only MSWindowsProcess::shutdown (the GUI and the service, to stop a core) does.
create_test(
  NAME ClientAppConnectTests
  DEPENDS client
  LIBS arch base io mt net platform server app ${extra_libs}
  SOURCE ClientAppConnectTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/client"
)

create_test(
  NAME ClientConnectDiagnosisTests
  DEPENDS client
  LIBS arch base io mt net platform server app ${extra_libs}
  SOURCE ClientConnectDiagnosisTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/client"
)

create_test(
  NAME ClipboardPacingTests
  DEPENDS client
  LIBS arch base io mt net platform server app ${extra_libs}
  SOURCE ClipboardPacingTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/client"
)
