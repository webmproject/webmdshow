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

update_libvpx_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --help: Display this message and exit.
    --git-hash <git hash/ref/tag>: Use this revision instead of HEAD.
    --keep-work-dir: Keep the work directory.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
EOF
}

trap cleanup EXIT

# Defaults for command line options.
KEEP_WORK_DIR=""
GIT_HASH="HEAD"

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
    --show-program-output)
      devnull=
      ;;
    --verbose)
      VERBOSE="yes"
      ;;
    *)
      update_libvpx_usage
      exit 1
      ;;
  esac
  shift
done

readonly LIBVPX_CONFIGURATIONS="x86-win32-vs12 x86_64-win64-vs12"
readonly LIBVPX_GIT_URL="https://chromium.googlesource.com/webm/libvpx"
readonly THIRD_PARTY="$(pwd)"
readonly WORK_DIR="tmp_$(date +%Y%m%d_%H%M%S)"

if [[ "${VERBOSE}" = "yes" ]]; then
cat << EOF
  KEEP_WORK_DIR=${KEEP_WORK_DIR}
  GIT_HASH=${GIT_HASH}
  VERBOSE=${VERBOSE}
  WORK_DIR=${WORK_DIR}
EOF
fi

# Bail out if not running from the third_party directory.
check_dir "third_party"

# Make a work directory, and cd into to avoid making a mess.
mkdir "${WORK_DIR}"
cd "${WORK_DIR}"

# Clone libvpx
eval git clone "${LIBVPX_GIT_URL}" ${devnull}

# Configure

# Build

# Install

# Readme snippet

vlog "Done."
