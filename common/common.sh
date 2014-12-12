#!/bin/sh
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
set -e
devnull='> /dev/null 2>&1'

readonly ORIG_PWD="$(pwd)"

elog() {
  echo "${0##*/} failed because: $@" 1>&2
}

vlog() {
  if [ "${VERBOSE}" = "yes" ]; then
    echo "$@"
  fi
}

# Terminates script when name of current directory does not match $1.
check_dir() {
  current_dir="$(pwd)"
  required_dir="$1"
  if [ "${current_dir##*/}" != "${required_dir}" ]; then
    elog "This script must be run from the ${required_dir} directory."
    exit 1
  fi
}

# Terminates the script when $1 is not in $PATH. Any arguments required for
# the tool being tested to return a successful exit code can be passed as
# additional arguments.
check_tool() {
  tool="$1"
  shift
  tool_args="$@"
  if ! eval "${tool}" ${tool_args} > /dev/null 2>&1; then
    elog "${tool} must be in your path."
    exit 1
  fi
}

# Terminates the script when cmake.exe is not in $PATH.
check_cmake() {
  check_tool "cmake.exe" "--version"
}

# Terminates the script when git.exe is not in $PATH.
check_git() {
  check_tool "git.exe" "--version"
}

# Terminates the script when make.exe is not in $PATH.
check_make() {
  check_tool "make.exe" "--version"
}

# Terminates the script when msbuild.exe is not in $PATH.
check_msbuild() {
  check_tool "msbuild.exe" "-help"
}

# Echoes git describe output for the directory specified by $1 to stdout.
git_describe() {
  git_dir="$1"
  check_git
  echo $(git -C "${git_dir}" describe)
}

# Echoes current git revision for the directory specifed by $1 to stdout.
git_revision() {
  git_dir="$1"
  check_git
  echo $(git -C "${git_dir}" rev-parse HEAD)
}

# Checks out the ref specified by $1 in the repo specified by $2.
git_checkout() {
  git_ref="$1"
  git_dir="$2"
  check_git
  eval git -C "${git_dir}" checkout "${git_ref}" ${devnull}
}
