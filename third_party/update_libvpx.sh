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
  local readonly res=$?
  cd "${ORIG_PWD}"

  if [[ $res -ne 0 ]]; then
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
    --make-jobs: Number of make jobs. Default is 1.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
EOF
}

# Builds and configures the libvpx target specified by $1 using configuration
# flags in $LIBVPX_CONFIGURE_FLAGS.
# Requires that $DIST_DIR is a non-empty environment variable.
build_libvpx_config() {
  local readonly config="$1"
  local readonly config_flags="--target=${config} ${LIBVPX_CONFIGURE_FLAGS[@]}"
  local readonly orig_pwd="$(pwd)"

  if [ -z "${DIST_DIR}" ]; then
    elog 'The $DIST_DIR environment variable must be non-empty.'
    exit 1
  fi

  mkdir -p "${LIBVPX}/${config}"
  cd "${LIBVPX}/${config}"

  # Configure for the specified target.
  eval ../configure ${config_flags} ${devnull}

  # Run make without the dist target. The dist target builds no libraries when
  # using MSVC.
  eval make -j "${MAKE_JOBS}" ${devnull}

  # Build the dist target.
  export DIST_DIR
  eval make -j "${MAKE_JOBS}" dist ${devnull}

  # Back to the original dir.
  cd "${orig_pwd}"
}

# Main worker function for this script. Builds all required libvpx configs,
# and installs includes/libs.
update_libvpx() {
  # Clone libvpx and checkout the desired ref.
  vlog "Cloning ${LIBVPX_GIT_URL}..."
  eval git clone "${LIBVPX_GIT_URL}" "${LIBVPX}" ${devnull}
  git_checkout "${GIT_HASH}" "${LIBVPX}"
  vlog "Done."

  # Configure and build each libvpx target.
  for target in ${LIBVPX_CONFIGURATIONS[@]}; do
    vlog "Building all configurations of target: ${target}..."
    build_libvpx_config "${target}"
    vlog "Done."
  done

  # Install the includes (only once; webmdshow doesn't use vpx_config/version.h,
  # so the includes are identical for all configurations). Just use the includes
  # for the most recently built target.
  vlog "Installing includes and libraries..."
  cp -p "${LIBVPX}/${target}/${DIST_DIR}"/include/vpx/*.h \
    "${ORIG_PWD}/libvpx/vpx/"

  # Copy libraries and PDB files to the appropriate locations.
  for (( i = 0; i < ${#LIBVPX_BUILD_LIB_DIRS[@]}; ++i )); do
    in_path="${LIBVPX}/${LIBVPX_BUILD_LIB_DIRS[$i]}"
    out_path="${ORIG_PWD}/libvpx/${LIBVPX_TARGET_LIB_DIRS[$i]}"

    eval cp -p "${in_path}"/vpx*.lib "${out_path}"
    eval cp -p "${in_path}"/vpx/*.pdb "${out_path}"
  done
  vlog "Done."
}

trap cleanup EXIT

# Defaults for command line options.
DIST_DIR="_dist"
GIT_HASH="HEAD"
HEADER_DIR="libvpx/vpx"
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
      update_libvpx_usage
      exit 1
      ;;
  esac
  shift
done

readonly LIBVPX="libvpx"
readonly LIBVPX_CONFIGURATIONS=(x86_64-win64-vs12 x86-win32-vs12)
readonly LIBVPX_BUILD_LIB_DIRS=(${LIBVPX_CONFIGURATIONS[0]}/x64/Debug
                                ${LIBVPX_CONFIGURATIONS[0]}/x64/Release
                                ${LIBVPX_CONFIGURATIONS[1]}/Win32/Debug
                                ${LIBVPX_CONFIGURATIONS[1]}/Win32/Release)
readonly LIBVPX_TARGET_LIB_DIRS=(x64/debug x64/release x86/debug x86/release)
readonly LIBVPX_CONFIGURE_FLAGS=(--enable-static-msvcrt
                                 --enable-libyuv
                                 --disable-docs
                                 --disable-examples
                                 --disable-unit-tests)
readonly LIBVPX_GIT_URL="https://chromium.googlesource.com/webm/libvpx"
readonly THIRD_PARTY="$(pwd)"
readonly WORK_DIR="tmp_$(date +%Y%m%d_%H%M%S)"

if [[ "${VERBOSE}" = "yes" ]]; then
cat << EOF
  KEEP_WORK_DIR=${KEEP_WORK_DIR}
  GIT_HASH=${GIT_HASH}
  MAKE_JOBS=${MAKE_JOBS}
  VERBOSE=${VERBOSE}
  WORK_DIR=${WORK_DIR}
  LIBVPX_CONFIGURATIONS=${LIBVPX_CONFIGURATIONS[@]}
  LIBVPX_BUILD_LIB_DIRS=${LIBVPX_BUILD_LIB_DIRS[@]}
  LIBVPX_TARGET_LIB_DIRS=${LIBVPX_TARGET_LIB_DIRS[@]}
  LIBVPX_CONFIGURE_FLAGS=${LIBVPX_CONFIGURE_FLAGS[@]}
  LIBVPX=${LIBVPX}
  THIRD_PARTY=${THIRD_PARTY}
EOF
fi

# Bail out if not running from the third_party directory.
check_dir "third_party"

# git.exe, make.exe, and msbuild.exe are ultimately required. Die when
# unavailable.
check_git
check_make
check_msbuild

# Make a work directory, and cd into to avoid making a mess.
mkdir "${WORK_DIR}"
cd "${WORK_DIR}"

# Clone/configure/build/install libvpx files.
update_libvpx

# Print the new snippet for README.webmdshow.
cat << EOF
Libvpx includes and libraries updated. README.webmdshow information follows:

libvpx
Version: $(git_describe "${LIBVPX}")
URL: ${LIBVPX_GIT_URL}

EOF

vlog "Done."
