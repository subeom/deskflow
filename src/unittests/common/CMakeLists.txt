# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME SettingsTests
  DEPENDS common
  SOURCE SettingsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)
# the test reads the declarations of the keys: the keys that are declared and not kept were taken away at every start
target_compile_definitions(SettingsTests PRIVATE DESKFLOW_SETTINGS_HEADER="${CMAKE_SOURCE_DIR}/src/lib/common/Settings.h")

create_test(
  NAME ConnectionDiagnosisTests
  DEPENDS common
  SOURCE ConnectionDiagnosisTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME LogLevelTests
  DEPENDS common
  SOURCE LogLevelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME I18NTests
  DEPENDS common
  SOURCE I18NTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME KeySequenceTests
  DEPENDS common
  SOURCE KeySequenceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME TunnelAddressTests
  DEPENDS common
  SOURCE TunnelAddressTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME TunnelReconnectTests
  DEPENDS common
  SOURCE TunnelReconnectTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)
