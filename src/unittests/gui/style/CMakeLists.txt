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
  NAME StatusColorsTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE StatusColorsTests.cpp
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

# The canvas of the arrangement and the monitor editor (phase 5)
create_test(
  NAME CanvasThemeTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE CanvasThemeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MenuStyleTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE MenuStyleTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME TextSizeModelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TextSizeModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TextSizeModelTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

# The fonts for the letters that the font of the system does not have (Hangul)
create_test(
  NAME FontPolicyTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE FontPolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(FontPolicyTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TextSizeTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TextSizeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TextSizeTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
