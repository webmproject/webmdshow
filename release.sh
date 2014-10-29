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

readonly ORIG_PATH="${PATH}"
readonly ORIG_PWD="$(pwd)"

elog() {
  echo "${0##*/} failed because: $@" 1>&2
}

vlog() {
  if [ "${VERBOSE}" = "yes" ]; then
    echo "$@"
  fi
}

cleanup() {
  cd "${ORIG_PWD}"

  if [ $? -ne 0 ]; then
    elog "cleanup() trapped error"
  fi

  if [ "${KEEP_RELEASE_DIR}" != "yes" ] && [ -n "${RELEASE_DIR}" ]; then
    rm -rf "${RELEASE_DIR}"
  fi

  if [ -n "${ORIG_PATH}" ]; then
    PATH="${ORIG_PATH}"
  fi
}

release_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --help: Display this message and exit.
    --7za-dir: Directory where 7za.exe is stored.
    --keep-release-dir: Keep the release directory after archiving.
    --nsis-dir: Directory where makensis.exe is stored.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
    --version: Dotted version number for this release (loaded from git repo
               if not specified).

    Note: When specifying directories, the unix path must be used. For
          example, the default makensis.exe (NSIS) install location is
          C:\\Program Files (x86)\\NSIS. The unix path is
          /c/Program Files (x86)/NSIS.

  Builds webmdshow-VERSION.zip:
  webmdshow-VERSION/
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

KEEP_RELEASE_DIR="yes"
NSIS_DIR="/c/Program Files (x86)/NSIS"
SEVENZA_DIR=""
WEBMDSHOW="webmdshow"

# Parse the command line.
while [ -n "$1" ]; do
  case "$1" in
   --help)
      release_usage
      exit
      ;;
    --7za-dir)
      SEVENZA_DIR="$2"
      shift
      ;;
    --keep-release-dir)
      KEEP_RELEASE_DIR="yes"
      ;;
    --nsis-dir)
      NSIS_DIR="$2"
      shift
      ;;
    --show-program-output)
      devnull=
      ;;
    --verbose)
      VERBOSE="yes"
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

readonly RELEASE_DIR="webmdshow-${VERSION}"
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
  7ZA_DIR=${SEVENZA_DIR}
  NSIS_DIR=${NSIS_DIR}
  RELEASE_DIR=${RELEASE_DIR}
  RELEASE_ZIP=${RELEASE_ZIP}
  VERSION=${VERSION}
  WEBMDSHOW=${WEBMDSHOW}
EOF
fi

# Create the empty directory structure for the archive.
vlog "*** Creating directories. ***"
eval mkdir -p ${RELEASE_DIR}/inc ${devnull}
eval mkdir -p ${RELEASE_DIR}/src ${devnull}

# Copy IDL files (and their generated .c and .h file counterparts) to
# ${RELEASE_DIR}/inc/.
vlog "*** Copying interface files. ***"
readonly IDL_BASE_NAMES="vp8decoder vp8encoder vp9decoder webmmux"
readonly IDL_DIR="${RELEASE_DIR}/inc"
for idlbase in ${IDL_BASE_NAMES}; do
  eval cp -p "${WEBMDSHOW}/IDL/${idlbase}.idl" "${IDL_DIR}" ${devnull}
  eval cp -p "${WEBMDSHOW}/IDL/${idlbase}idl.c" "${IDL_DIR}" ${devnull}
  eval cp -p "${WEBMDSHOW}/IDL/${idlbase}idl.h" "${IDL_DIR}" ${devnull}
done

# Copy sources for Vorbis and WebM types.
vlog "*** Copying source files. ***"
readonly SRC_BASE_NAMES="vorbistypes webmtypes"
readonly SRC_DIR="${RELEASE_DIR}/src"
for srcbase in ${SRC_BASE_NAMES}; do
  eval cp -p "${WEBMDSHOW}/common/${srcbase}.cpp" "${SRC_DIR}" ${devnull}
  eval cp -p "${WEBMDSHOW}/common/${srcbase}.hpp" "${SRC_DIR}" ${devnull}
done

# Copy executables.
vlog "*** Copying executables. ***"
readonly EXE_FILES="makewebm.exe playwebm.exe"
readonly EXE_BUILD_DIR="exe/webmdshow/Release"
for exe in ${EXE_FILES}; do
  eval cp -p "${EXE_BUILD_DIR}/${exe}" "${RELEASE_DIR}" ${devnull}
done

# Copy text files.
vlog "*** Copying text files. ***"
readonly TEXT_FILES="AUTHORS.TXT LICENSE.TXT PATENTS.TXT README.TXT"
for text_file in ${TEXT_FILES}; do
  eval cp -p "${WEBMDSHOW}/${text_file}" "${RELEASE_DIR}" ${devnull}
done

# Build the installer and copy it to ${RELEASE_DIR}.
vlog "*** Building installer. ***"
if [ -n "${NSIS_DIR}" ] && [ -d "${NSIS_DIR}" ]; then
  PATH="${NSIS_DIR}:${PATH}"
fi
readonly NSIS="makensis.exe"
readonly INSTALLER_DIR="${WEBMDSHOW}/installer"
readonly INSTALLER="install_webmdshow.exe"
if [ -e "${INSTALLER_DIR}/${INSTALLER}" ]; then
  eval rm -f "${INSTALLER_DIR}/${INSTALLER}" ${devnull}
fi
eval ${NSIS} "${INSTALLER_DIR}/webmdshow.nsi" ${devnull}
if [ ! -e "${INSTALLER_DIR}/${INSTALLER}" ]; then
  elog "Installer build failed."
  exit 1
fi
eval mv "${INSTALLER_DIR}/${INSTALLER}" "${RELEASE_DIR}" ${devnull}

# Create the archive.
vlog "*** Creating ${RELEASE_ZIP} ***"
if [ -n "$SEVENZA_DIR}" ] && [ -d "${SEVENZA_DIR}" ]; then
  PATH="${SEVENZA_DIR}:${PATH}"
fi
readonly SEVENZA="7za.exe"
eval ${SEVENZA} a -tzip "${RELEASE_ZIP}" "${RELEASE_DIR}" ${devnull}

vlog "Done."
