#!/bin/bash
##
##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
. $(dirname $0)/common/common.sh

cleanup() {
  local readonly res=$?
  cd "${ORIG_PWD}"

  if [[ $res -ne 0 ]]; then
    elog "cleanup() trapped error"
  fi
}

set_version_usage() {
cat << EOF
  Usage: ${0##*/} --mode <dshow or mediafoundation> --version <version> [args]
    --help: Display this message and exit.
    --mode: Which version numbers to update.
    --version: Version number to use. Must be a dotted quad.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
EOF
}

readonly DSHOW_RCFILES="makewebm
                        playwebm
                        vp8decoder
                        vp8encoder
                        vp9decoder
                        vpxdecoder
                        webmmux
                        webmsource
                        webmsplit
                        webmcc
                        webmvorbisencoder
                        webmvorbisdecoder
                        webmoggsource"
readonly MF_RCFILES="webmmfsource
                     webmmfvorbisdec
                     webmmfvp8dec"

# Parse the command line.
while [[ -n "$1" ]]; do
  case "$1" in
    --help)
      set_version_usage
      exit
      ;;
    --mode)
      MODE="$2"
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
     *)
      set_version_usage
      exit 1
      ;;
  esac
  shift
done

if [[ -z "${MODE}" ]] || [[ -z "${VERSION}" ]]; then
  elog "The mode and version arguments are required."
  exit 1
fi

# Make sure the version parameter is a dotted quad.
if ! echo "${VERSION}" \
  | egrep '[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*' > /dev/null 2>&1; then
  elog "The version argument must be a dotted quad."
  exit 1
fi

# A comma separated version number is needed in addition to the dotted one.
VERSION_COMMAD="$(echo "${VERSION}" | tr "." ",")"

DIRECTORY="."
RC_FILES="${DSHOW_RCFILES}"
if [[ "${MODE}" == "mediafoundation" ]]; then
  DIRECTORY="mediafoundation"
  RC_FILES="${MF_RCFILES}"
fi

readonly INDENT="           "

for rc_file in ${RC_FILES}; do
  rc_file="${DIRECTORY}/${rc_file}/${rc_file}.rc"
  vlog "Updating ${rc_file}..."
  sed -i \
    -e "/FILEVERSION/ c\ FILEVERSION ${VERSION_COMMAD}" \
    -e "/PRODUCTVERSION/ c\ PRODUCTVERSION ${VERSION_COMMAD}" \
    -e "/FileVersion/ c\ ${INDENT}VALUE \"FileVersion\", \"${VERSION}\"" \
    -e "/ProductVersion/ c\ ${INDENT}VALUE \"ProductVersion\", \"${VERSION}\"" \
    "${rc_file}"
done

vlog "Done."

