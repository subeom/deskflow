# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# A stand-in for cloudflared that the tests of the tunnel start. Its name starts with "cloudflared" because the cleanup of
# a leftover process only ends a program that is called that. It is never installed.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_executable(cloudflared-fake FakeCloudflared.cpp)
target_link_libraries(cloudflared-fake PRIVATE Qt6::Core Qt6::Network)
