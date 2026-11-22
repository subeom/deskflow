# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME InputIndicatorPolicyTests
  DEPENDS gui
  SOURCE InputIndicatorPolicyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME InputIndicatorGeometryTests
  DEPENDS gui
  SOURCE InputIndicatorGeometryTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
