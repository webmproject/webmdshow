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

# Trap function. Cleans up the subtree used to build all targets contained in
# $TARGETS.
cleanup() {
  cd "${ORIG_PWD}"

  if [ $? -ne 0 ]; then
    elog "cleanup() trapped error"
  fi
}

release_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --version: Dotted version number for this release (loaded from git repo
               if not specified).
    --help: Display this message and exit.
    --preserve-build-output: Do not delete the build directory.
    --show-program-output: Show output from each step.
    --verbose: Show more output.

  Builds webmdshow_VERSION.zip:
  webmdshow_VERSION/
    inc/
      vp8decoder.idl
      vp8decoderidl.c
      vp8decoderidl.h
      vp8encoder.idl
      vp8encoderidl.c
      vp8encoderidl.h
      vp9decoder.idl
      vp9decoderidl.c
      vp9decoderidl.h
      webmmux.idl
      webmmuxidl.c
      webmmuxidl.h
    src/
      vorbistypes.cpp
      vorbistypes.hpp
      webmtypes.cpp
      webmtypes.hpp
    AUTHORS.TXT
    LICENSE.TXT
    PATENTS.TXT
    README.TXT
    install_webmdshow.exe
    makewebm.exe
    playwebm.exe

  This script must be run from the directory containing the webmdshow
  repository from which the release was built.
EOF
}

trap cleanup EXIT
WEBMDSHOW=webmdshow

# Parse the command line.
while [ -n "$1" ]; do
  case "$1" in
   --help)
      release_usage
      exit
      ;;
    --show-program-output)
      devnull=
      ;;
    --verbose)
      VERBOSE=yes
      ;;
    --version)
      VERSION="$2"
      shift
      ;;
    --webmdshow)
      WEBMDSHOW="$2"
      shift
      ;;
     *)
      release_usage
      exit 1
      ;;
  esac
  shift
done

if [ -z "${VERSION}" ]; then
  cd "${WEBMDSHOW}"
  VERSION="$(git tag | grep webmdshow | tail -n1 | tr -d "webmdshow-")"
  cd "${ORIG_PWD}"
fi

if [ -z "${VERSION}" ]; then
  elog "Unable to read version from webmdshow repo and none provided."
  exit 1
fi

readonly RELEASE_DIR="webmdshow_${VERSION}"
readonly RELEASE_ZIP="${RELEASE_DIR}.zip"

if [ -z "${RELEASE_DIR}" ] || [ -z "${RELEASE_ZIP}" ]; then
  elog "Empty release archive and/or directory names."
  exit 1
fi

if [ ! -d "${WEBMDSHOW}" ] || [ ! -d "dll" ] || [ ! -d "exe" ]; then
  elog "Missing webmdshow and/or dll and/or exe directory."
  release_usage
  exit
fi

if [ "${VERBOSE}" = "yes" ]; then
cat <<EOF
  RELEASE_DIR=${RELEASE_DIR}
  RELEASE_ZIP=${RELEASE_ZIP}
  VERSION=${VERSION}
  WEBMDSHOW=${WEBMDSHOW}
EOF
fi

# Create the empty directory structure for the archive.
eval mkdir -p ${RELEASE_DIR} ${devnull}
eval mkdir -p ${RELEASE_DIR}/inc ${devnull}
eval mkdir -p ${RELEASE_DIR}/src ${devnull}

# Copy IDL files (and their generated .c and .h file counterparts) to
# ${RELEASE_DIR}/inc/.
readonly IDL_BASE_NAMES="vp8decoder vp8encoder vp9decoder webmmux"
readonly IDL_DIR="${RELEASE_DIR}/inc"
for idlbase in ${IDL_BASE_NAMES}; do
  eval cp -p "${WEBMDSHOW}/IDL/${idlbase}.idl" "${IDL_DIR}" ${devnull}
  eval cp -p "${WEBMDSHOW}/IDL/${idlbase}idl.c" "${IDL_DIR}" ${devnull}
  eval cp -p "${WEBMDSHOW}/IDL/${idlbase}idl.h" "${IDL_DIR}" ${devnull}
done

# Copy sources for Vorbis and WebM types.
readonly SRC_BASE_NAMES="vorbistypes webmtypes"
readonly SRC_DIR="${RELEASE_DIR}/src"
for srcbase in ${SRC_BASE_NAMES}; do
  eval cp -p "${WEBMDSHOW}/common/${srcbase}.cpp" "${SRC_DIR}" ${devnull}
  eval cp -p "${WEBMDSHOW}/common/${srcbase}.hpp" "${SRC_DIR}" ${devnull}
done

# Copy executables.
readonly EXE_FILES="makewebm.exe playwebm.exe"
readonly EXE_BUILD_DIR="exe/webmdshow/Release"
for exe in ${EXE_FILES}; do
  eval cp -p "${EXE_BUILD_DIR}/${exe}" "${RELEASE_DIR}" ${devnull}
done

# Copy text files.
readonly TEXT_FILES="AUTHORS.TXT LICENSE.TXT PATENTS.TXT README.TXT"
for text_file in ${TEXT_FILES}; do
  eval cp -p "${WEBMDSHOW}/${text_file}" "${RELEASE_DIR}" ${devnull}
done

# Build the installer and copy it to ${RELEASE_DIR}.

# Create the archive.

