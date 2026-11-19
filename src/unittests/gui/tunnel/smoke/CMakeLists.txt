# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# The manual check of the secure store of the system. Not a test (it is not run by ctest): see SecretStoreSmoke.cpp.
add_executable(secret-store-smoke SecretStoreSmoke.cpp)
target_link_libraries(secret-store-smoke PRIVATE gui Qt6::Core)
