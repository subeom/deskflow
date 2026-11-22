# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

# The tests that link the server library (the configuration of the server, the arrangement, the monitor editor) reach
# objects of it that need the library of the program (KeyMap, IKeyState, ClipboardSharePolicy). The server library links
# that library on Unix only, so on Windows these tests name what the tests of the server name (see ../server).
if(WIN32)
  set(gui_server_libs base arch version app mt net)
endif()

add_subdirectory(awdl)
add_subdirectory(chrome)
add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(dialogs)
add_subdirectory(input)
add_subdirectory(layout)
add_subdirectory(settings)
add_subdirectory(shell)
add_subdirectory(style)
add_subdirectory(tray)
add_subdirectory(widgets)

# the Cloudflare Tunnel
add_subdirectory(tunnel)

create_test(
  NAME LoggerTests
  DEPENDS gui
  SOURCE LoggerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME UpdateReleaseTests
  DEPENDS gui
  SOURCE UpdateReleaseTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME UpdateNetworkTests
  DEPENDS gui
  SOURCE UpdateNetworkTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME UpdateIntegrityTests
  DEPENDS gui
  SOURCE UpdateIntegrityTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME UpdateCacheTests
  DEPENDS gui
  SOURCE UpdateCacheTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME VersionCheckerTests
  DEPENDS gui
  SOURCE VersionCheckerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME UpdateStatusModelTests
  DEPENDS gui
  SOURCE UpdateStatusModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

if(APPLE)
  if(BUILD_INSTALLER)
    create_test(
      NAME MacUpdatePackagingTests
      DEPENDS gui
      SOURCE MacUpdatePackagingTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
    )
    target_compile_definitions(
      MacUpdatePackagingTests
      PRIVATE DESKFLOW_CPACK_CONFIG="${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    )
  endif()
  create_test(
    NAME UpdateVerifierTests
    DEPENDS gui
    SOURCE UpdateVerifierTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )

  create_test(
    NAME MacUpdateIntegrationTests
    DEPENDS gui
    SOURCE MacUpdateIntegrationTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )

  create_test(
    NAME UpdateInstallerTests
    DEPENDS gui
    SOURCE UpdateInstallerTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )
endif()

create_test(
  NAME UpdateShutdownTests
  DEPENDS gui
  SOURCE UpdateShutdownTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
