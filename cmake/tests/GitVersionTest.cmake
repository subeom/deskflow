# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# Checks deskflow_git_version() on scratch repositories:
#   cmake -DGIT_VERSION_CMAKE=<cmake/GitVersion.cmake> -DWORK_DIR=<empty scratch dir> -P GitVersionTest.cmake
# A repository that has the tag v1.27.0.45 (a build tagged by hand) must read the same version as one that does not.

cmake_minimum_required(VERSION 3.24)
include("${GIT_VERSION_CMAKE}")

find_package(Git REQUIRED)

if(NOT WORK_DIR)
  message(FATAL_ERROR "WORK_DIR is the scratch directory of the repositories")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(git repo)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -c user.name=test -c user.email=test@example.invalid -c commit.gpgsign=false
            -c tag.gpgsign=false ${ARGN}
    WORKING_DIRECTORY "${repo}"
    RESULT_VARIABLE result
    OUTPUT_QUIET ERROR_VARIABLE error
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "git ${ARGN} failed: ${error}")
  endif()
endfunction()

function(commits repo count)
  foreach(i RANGE 1 ${count})
    git("${repo}" commit --allow-empty -m "commit ${i}")
  endforeach()
endfunction()

function(expect repo expected)
  # what a caller starts from, which only a checkout that has a release tag changes
  set(DESKFLOW_VERSION_MAJOR 1)
  set(DESKFLOW_VERSION_MINOR 27)
  set(DESKFLOW_VERSION_PATCH 0)
  set(DESKFLOW_VERSION_TWEAK 0)
  deskflow_git_version("${repo}")
  set(actual "${DESKFLOW_VERSION_MAJOR}.${DESKFLOW_VERSION_MINOR}.${DESKFLOW_VERSION_PATCH}.${DESKFLOW_VERSION_TWEAK}")
  if(NOT actual STREQUAL expected)
    message(SEND_ERROR "${repo}: expected ${expected}, got ${actual}")
  else()
    message(STATUS "ok: ${actual}")
  endif()
endfunction()

function(new_repo name)
  set(repo "${WORK_DIR}/${name}")
  file(MAKE_DIRECTORY "${repo}")
  git("${repo}" init -q)
  set(repo "${repo}" PARENT_SCOPE)
endfunction()

# no tag: the tweak that says so, the numbers of the caller
new_repo(untagged)
commits("${repo}" 2)
expect("${repo}" "1.27.0.9999")

# the release tag and three commits after it
new_repo(plain)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0 -m release)
commits("${repo}" 3)
expect("${repo}" "1.27.0.3")

# exactly on the release
new_repo(on_release)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0 -m release)
expect("${repo}" "1.27.0.0")

# the same history, and a tag of four numbers on the way (as a Windows build was tagged): the same version
new_repo(four_number_tag)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0 -m release)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0.45 -m "an installed build")
commits("${repo}" 2)
expect("${repo}" "1.27.0.3")

# HEAD is the commit of the four-number tag
new_repo(on_four_number_tag)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0 -m release)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0.45 -m "an installed build")
expect("${repo}" "1.27.0.1")

# a tag of another shape (a pre-release, another prefix) is not a release
new_repo(other_tags)
commits("${repo}" 1)
git("${repo}" tag -a v1.27.0 -m release)
commits("${repo}" 1)
git("${repo}" tag -a v1.28.0-beta.1 -m "not a release")
git("${repo}" tag -a build-77 -m "not a release")
commits("${repo}" 1)
expect("${repo}" "1.27.0.2")
