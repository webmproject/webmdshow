#!/bin/bash
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
. $(dirname $0)/../common/common.sh

cleanup() {
  cd "${ORIG_PWD}"

  if [[ $? -ne 0 ]]; then
    elog "cleanup() trapped error"
  fi
  if [[ "${KEEP_WORK_DIR}" != "yes" ]]; then
    if [[ -n "${WORK_DIR}" ]] && [[ -d "${WORK_DIR}" ]]; then
      rm -rf "${WORK_DIR}"
    fi
  fi
}

update_libyuv_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --help: Display this message and exit.
    --git-hash <git hash/ref/tag>: Use this revision instead of HEAD.
    --keep-work-dir: Keep the work directory.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
EOF
}

# Runs cmake to generate projects for the MSVC IDE version specified by
# $1 from the CMakeLists.txt in the directory specified by $2.
cmake_generate_projects() {
  local ide_version="$1"
  local cmake_lists_dir="$2"
  local config_subdir="$3"
  local orig_pwd="$(pwd)"

  mkdir -p "${config_subdir}"
  cd "${config_subdir}"
  eval cmake.exe "${cmake_lists_dir}" "${ide_version}" ${devnull}
  cd "${orig_pwd}"
}

# Defaults for command line options.
GIT_HASH="HEAD"
KEEP_WORK_DIR=""
MAKE_JOBS="1"

# Parse the command line.
while [[ -n "$1" ]]; do
  case "$1" in
    --help)
      update_xiph_libs_usage
      exit
      ;;
    --git-hash)
      GIT_HASH="$2"
      shift
      ;;
    --keep-work-dir)
      KEEP_WORK_DIR="yes"
      ;;
    --make-jobs)
      MAKE_JOBS="$2"
      shift
      ;;
    --show-program-output)
      devnull=
      ;;
    --verbose)
      VERBOSE="yes"
      ;;
    *)
      update_libyuv_usage
      exit 1
      ;;
  esac
  shift
done

readonly CMAKE_BUILD_DIRS=(Win32/Debug Win32/RelWithDebInfo
                           x64/Debug x64/RelWithDebInfo)
readonly CMAKE_MSVC_GENERATORS=("Visual Studio 12"
                                "Visual Studio 12 Win64")
readonly LIBYUV="libyuv"
readonly LIBYUV_CONFIGS=(x64 x86)
readonly LIBYUV_GIT_URL="https://chromium.googlesource.com/external/libyuv"
readonly LIBYUV_HEADER_DIR="libyuv/include"
readonly LIBYUV_TARGET_LIB_DIRS=(x64/debug x64/release x86/debug x86/release)
readonly THIRD_PARTY="$(pwd)"
readonly WORK_DIR="tmp_$(date +%Y%m%d_%H%M%S)"

if [[ "${VERBOSE}" = "yes" ]]; then
cat << EOF
  KEEP_WORK_DIR=${KEEP_WORK_DIR}
  GIT_HASH=${GIT_HASH}
  MAKE_JOBS=${MAKE_JOBS}
  VERBOSE=${VERBOSE}
  CMAKE_BUILD_DIRS=${CMAKE_BUILD_DIRS[@]}
  CMAKE_MSVC_GENERATORS=${CMAKE_MSVC_GENERATORS[@]}
  LIBYUV=${LIBYUV}
  LIBYUV_CONFIGS=${LIBYUV_CONFIGS[@]}
  LIBYUV_GIT_URL=${LIBYUV_GIT_URL}
  LIBYUV_HEADER_DIR=${LIBYUV_HEADER_DIR}
  LIBYUV_TARGET_LIB_DIRS=${LIBYUV_TARGET_LIB_DIRS[@]}
  THIRD_PARTY=${THIRD_PARTY}
  WORK_DIR=${WORK_DIR}
EOF
fi

# Bail out if not running from the third_party directory.
check_dir "third_party"

# cmake.exe, git.exe, and msbuild.exe are ultimately required. Die when
# unavailable.
check_cmake
check_git
check_msbuild

# Make a work directory, and cd into to avoid making a mess.
mkdir "${WORK_DIR}"
cd "${WORK_DIR}"

update_libyuv() {
  # Clone libyuv and checkout the desired ref.
  vlog "Cloning ${LIBYUV_GIT_URL}..."
  eval git clone "${LIBYUV_GIT_URL}" "${LIBYUV}" ${devnull}
  git_checkout "${GIT_HASH}" "${LIBYUV}"
  vlog "Done."

  for generator in "${CMAKE_MSVC_GENERATORS[@]}" do
    cmake_generate_projects "${generator}" ".." 
  done
}
