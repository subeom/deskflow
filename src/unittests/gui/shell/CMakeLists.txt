# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME StatusModelTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE StatusModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME MainShellTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE MainShellTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the header of the main window includes the headers next to it by their short names
target_include_directories(MainShellTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TextSizeShellTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TextSizeShellTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TextSizeShellTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME TunnelShellTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TunnelShellTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(TunnelShellTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

# The new look in Korean with the translation that is shipped: the .qm that the build makes of translations/deskflow_ko.ts
# (the test builds it first), and the file of the translation itself
create_test(
  NAME KoreanLookTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE KoreanLookTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(KoreanLookTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
target_compile_definitions(KoreanLookTests PRIVATE DESKFLOW_KO_QM="${CMAKE_BINARY_DIR}/translations/deskflow_ko.qm")
add_dependencies(KoreanLookTests app_translations)

# The main window of the new look at every size that it can have, page by page, in English and in Korean (the .qm that
# is shipped): nothing covers a sibling, nothing is cut off, nothing has less room than it asks for
create_test(
  NAME LayoutAuditTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE LayoutAuditTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(LayoutAuditTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
target_compile_definitions(LayoutAuditTests PRIVATE DESKFLOW_KO_QM="${CMAKE_BINARY_DIR}/translations/deskflow_ko.qm")
add_dependencies(LayoutAuditTests app_translations)

create_test(
  NAME TranslationFileTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE TranslationFileTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_compile_definitions(TranslationFileTests PRIVATE DESKFLOW_KO_TS="${CMAKE_SOURCE_DIR}/translations/deskflow_ko.ts")
add_dependencies(TranslationFileTests app_translations)

# The option that turns off the wireless link of AirDrop while sharing (macOS) in the main window: what it asks of the
# system (a fake) and when, the banner, and nothing at all where there is no such link
create_test(
  NAME AwdlShellTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE AwdlShellTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(AwdlShellTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
