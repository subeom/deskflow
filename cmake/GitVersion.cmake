# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# Reads the version of a git checkout from its tags.
#
# deskflow_git_version(<source dir>)
#
# When <source dir> is a git checkout and git is found, sets DESKFLOW_VERSION_MAJOR, _MINOR, _PATCH and _TWEAK and
# GIT_SHA_SHORT in the scope of the caller. The version is that of the last release tag, vMAJOR.MINOR.PATCH, and the
# number of commits since it is the tweak: "v1.27.0-90-g1a2b3c4d" is 1.27.0.90. A checkout that has no such tag has the
# tweak 9999. When it is not a checkout, or git is missing, nothing is set, and the caller's defaults stay.
#
# Tags of any other shape are not a release and are never used. In particular a tag of four numbers, as a build that
# was made from a checkout may have been tagged with (v1.27.0.45), would be read as the version 1.27.45 by the
# patterns below: the clone that has it would not be the version of a clone that does not, and a Windows and a macOS
# build of one commit would not report the same version.
function(deskflow_git_version SOURCE_DIR)
  if(NOT EXISTS "${SOURCE_DIR}/.git")
    return()
  endif()

  find_package(Git)
  if(NOT GIT_FOUND)
    return()
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_SHA_SHORT
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(GIT_SHA_SHORT "${GIT_SHA_SHORT}" PARENT_SCOPE)

  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --long --match "v[0-9]*" --exclude "v*.*.*.*" --always
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GITREV
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(FIND "${GITREV}" "v" isRev)
  if(NOT ${isRev} EQUAL -1)
    string(REGEX MATCH [0-9]+ MAJOR ${GITREV})
    string(REGEX MATCH \\.[0-9]+ MINOR ${GITREV})
    string(REPLACE "." "" MINOR "${MINOR}")
    string(REGEX MATCH [0-9]+\- PATCH ${GITREV})
    string(REPLACE "-" "" PATCH "${PATCH}")
    string(REGEX MATCH \-[0-9]+\- TWEAK ${GITREV})
    string(REPLACE "-" "" TWEAK "${TWEAK}")
    set(DESKFLOW_VERSION_MAJOR ${MAJOR} PARENT_SCOPE)
    set(DESKFLOW_VERSION_MINOR ${MINOR} PARENT_SCOPE)
    set(DESKFLOW_VERSION_PATCH ${PATCH} PARENT_SCOPE)
    set(DESKFLOW_VERSION_TWEAK ${TWEAK} PARENT_SCOPE)
  else()
    set(DESKFLOW_VERSION_TWEAK "9999" PARENT_SCOPE)
  endif()
endfunction()
