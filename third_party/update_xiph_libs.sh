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

update_xiph_libs_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --help: Display this message and exit.
    --keep-work-dir: Keep the work directory.
    --ogg-version: Dotted version number of libogg release to download.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
    --vorbis-version: Dotted version number of libvorbis release to download.
EOF
}

curl_download() {
  eval curl -O "$@" ${devnull}
}

wget_download() {
  eval wget "$@" ${devnull}
}

download() {
  vlog "Downloading $1..."
  ${DOWNLOADER} "$@"
  vlog "Done."
}

decompress_7za() {
  eval 7za x "$@" ${devnull}
}

decompress_unzip() {
  eval unzip "$@" ${devnull}
}

decompress() {
  vlog "Decompressing $1..."
  ${DECOMPRESS} "$@"
  vlog "Done."
}

trap cleanup EXIT

# Picks curl or wget (or terminates the script with an error) and echoes the
# appropriate wrapper function to stdout.
# Note: Since the caller relies on stdout to read the result of this shell
#       function --show-program-output has no effect here.
#
pick_download_wrapper() {
  if curl --version > /dev/null 2>&1; then
    echo curl_download
    return
  fi
  if wget --version > /dev/null 2>&1; then
    echo wget_download
    return
  fi
  elog "curl or wget must be in your path."
  exit 1
}

pick_decompress_wrapper() {
  if unzip > /dev/null 2>&1; then
    echo decompress_unzip
    return
  fi
  if 7za > /dev/null 2>&1; then
    echo decompress_7za
    return
  fi
  elog "7za or unzip must be in your path."
  exit 1
}

build_libs() {
  vcxproj="$1"
  vlog "Building ${vcxproj}..."
  for config in ${MSBUILD_CONFIGURATIONS}; do
    for platform in ${MSBUILD_PLATFORMS}; do
      eval msbuild.exe "${vcxproj}" \
        -p:Configuration="${config}" \
        -p:Platform="$platform" \
        -p:PlatformToolset="${MSBUILD_TOOLSET}" \
        ${devnull}
    done
  done
  vlog "Done."
}

# Enable PDB generation for libogg release configs. For reasons unknown attempts
# to enable this via the msbuild.exe command line have no effect.
enable_libogg_release_pdb_generation() {
  libogg_vcxproj="${MSBUILD_OGG_PATH}/${MSBUILD_OGG_PROJECT}"
  tag_open='\<DebugInformationFormat\>'
  tag_close='\</DebugInformationFormat\>'
  # Note: ':' is the separator in the first two expressions.
  eval sed -i \
    -e "s:^[[:space:]]*${tag_close}\$::" \
    -e "s:${tag_open}\$:${tag_open}ProgramDatabase${tag_close}:" \
    -e "s/EditAndContinue/ProgramDatabase/" \
    "${libogg_vcxproj}" ${devnull}
}

# Apply two fixes to libvorbis_static.vcxproj:
# - Really produce static libs.
# - Set all debug information formats to ProgramDatabase (silences an
#   EditAndContinue related warning).
fix_libvorbis_vcxproj() {
  libvorbis_vcxproj="${MSBUILD_VORBIS_PATH}/${MSBUILD_VORBIS_PROJECT}"
  eval sed -i \
    -e "s/MultiThreadedDebugDLL/MultiThreadedDebug/" \
    -e "s/MultiThreadedDLL/MultiThreaded/" \
    -e "s/EditAndContinue/ProgramDatabase/" \
    "${libvorbis_vcxproj}" ${devnull}
}

install_ogg_files() {
  cp -p libogg/include/ogg/*.h ../libogg/ogg/
  for platform in ${MSBUILD_PLATFORMS}; do
    for config in ${MSBUILD_CONFIGURATIONS}; do
      ogg_lib_dirs="${ogg_lib_dirs} ${MSBUILD_OGG_PATH}/${platform}/${config}"
    done
  done

  for platform in ${WEBMDSHOW_PLATFORMS}; do
    for config in ${WEBMDSHOW_CONFIGURATIONS}; do
      webmdshow_dirs="${webmdshow_dirs} ../libogg/${platform}/${config}"
    done
  done

  ogg_dir_array=(${ogg_lib_dirs})
  webmdshow_dir_array=(${webmdshow_dirs})

  for (( i = 0; i < ${#ogg_dir_array[@]}; ++i )); do
    cp -p "${ogg_dir_array[$i]}/libogg_static.lib" "${webmdshow_dir_array[$i]}"
    cp -p "${ogg_dir_array[$i]}/vc120.pdb" "${webmdshow_dir_array[$i]}"
  done
}

install_vorbis_files() {
  cp -p libvorbis/include/vorbis/*.h ../libvorbis/vorbis/
  for platform in ${MSBUILD_PLATFORMS}; do
    for config in ${MSBUILD_CONFIGURATIONS}; do
      vorbis_dirs="${vorbis_dirs} ${MSBUILD_VORBIS_PATH}/${platform}/${config}"
    done
  done

  webmdshow_dirs=""
  for platform in ${WEBMDSHOW_PLATFORMS}; do
    for config in ${WEBMDSHOW_CONFIGURATIONS}; do
      webmdshow_dirs="${webmdshow_dirs} ../libvorbis/${platform}/${config}"
    done
  done

  vorbis_dir_array=(${vorbis_dirs})
  webmdshow_dir_array=(${webmdshow_dirs})

  for (( i = 0; i < ${#vorbis_dir_array[@]}; ++i )); do
    eval cp -p "${vorbis_dir_array[$i]}/libvorbis_static.lib" \
      "${webmdshow_dir_array[$i]}" ${devnull}
    eval cp -p "${vorbis_dir_array[$i]}/vc120.pdb" \
      "${webmdshow_dir_array[$i]}" ${devnull}
  done
}

# Defaults for command line options.
KEEP_WORK_DIR=""
OGG_VERSION="1.3.2"
VORBIS_VERSION="1.3.4"

# Parse the command line.
while [[ -n "$1" ]]; do
  case "$1" in
    --help)
      update_xiph_libs_usage
      exit
      ;;
    --keep-work-dir)
      KEEP_WORK_DIR="yes"
      ;;
    --ogg-version)
      OGG_VERSION="$2"
      shift
      ;;
    --show-program-output)
      devnull=
      ;;
    --verbose)
      VERBOSE="yes"
      ;;
    --vorbis-version)
      VORBIS_VERSION="$2"
      shift
      ;;
    *)
      update_xip_libs_usage
      exit 1
      ;;
  esac
  shift
done

# The order of terms, their meanings specifically, in $MSBUILD_CONFIGURATIONS
# and $MSBUILD_PLATFORMS must match the order in $WEBMDSHOW_CONFIGURATIONS and
# $WEBMDSHOW_PLATFORMS. The actual term names differ.
readonly MSBUILD_CONFIGURATIONS="Debug Release"
readonly WEBMDSHOW_CONFIGURATIONS="debug release"
readonly MSBUILD_PLATFORMS="Win32 x64"
readonly WEBMDSHOW_PLATFORMS="x86 x64"

readonly MSBUILD_OGG_PATH="libogg/win32/VS2010"
readonly MSBUILD_OGG_PROJECT="libogg_static.vcxproj"
readonly MSBUILD_VORBIS_PATH="libvorbis/win32/VS2010/libvorbis"
readonly MSBUILD_VORBIS_PROJECT="libvorbis_static.vcxproj"
readonly MSBUILD_TOOLSET="v120"
readonly THIRD_PARTY="$(pwd)"
readonly WORK_DIR="tmp_$(date +%Y%m%d_%H%M%S)"
readonly LIBOGG_ARCHIVE="libogg-${OGG_VERSION}.zip"
readonly LIBVORBIS_ARCHIVE="libvorbis-${VORBIS_VERSION}.zip"
readonly XIPH_DL_SERVER="http://downloads.xiph.org"
readonly XIPH_DL_LIBOGG="releases/ogg/${LIBOGG_ARCHIVE}"
readonly XIPH_DL_LIBVORBIS="releases/vorbis/${LIBVORBIS_ARCHIVE}"

if [[ "${VERBOSE}" = "yes" ]]; then
cat << EOF
  KEEP_WORK_DIR=${KEEP_WORK_DIR}
  OGG_VERSION=${OGG_VERSION}
  LIBOGG_ARCHIVE=${LIBOGG_ARCHIVE}
  LIBVORBIS_ARCHIVE=${LIBVORBIS_ARCHIVE}
  MSBUILD_CONFIGURATIONS=${MSBUILD_CONFIGURATIONS}
  MSBUILD_PLATFORMS=${MSBUILD_PLATFORMS}
  MSBUILD_TOOLSET=${MSBUILD_TOOLSET}
  VERBOSE=${VERBOSE}
  VORBIS_VERSION=${VORBIS_VERSION}
  WORK_DIR=${WORK_DIR}
  XIPH_DL_SERVER=${XIPH_DL_SERVER}
  XIPH_DL_LIBOGG=${XIPH_DL_LIBOGG}
  XIPH_DL_LIBVORBIS=${XIPH_DL_LIBVORBIS}
EOF
fi

# Pick utilities needed to update.
readonly DOWNLOADER="$(pick_download_wrapper)"
readonly DECOMPRESS="$(pick_decompress_wrapper)"

# Bail out if not running from the third_party directory.
check_dir "third_party"

# Bail out if msbuild.exe is not available.
check_msbuild

# Make a work directory, and cd into to avoid making a mess.
mkdir "${WORK_DIR}"
cd "${WORK_DIR}"

# Download release archives.
download "${XIPH_DL_SERVER}/${XIPH_DL_LIBOGG}"
download "${XIPH_DL_SERVER}/${XIPH_DL_LIBVORBIS}"

# Decompress the archives.
decompress "${LIBOGG_ARCHIVE}"
decompress "${LIBVORBIS_ARCHIVE}"

# Strip the version number from directory names to avoid include and library
# path related compile and link errors.
mv "$(basename ${LIBOGG_ARCHIVE} .zip)" libogg
mv "$(basename ${LIBVORBIS_ARCHIVE} .zip)" libvorbis

# Build the Ogg and Vorbis libraries.
enable_libogg_release_pdb_generation
build_libs "${MSBUILD_OGG_PATH}/${MSBUILD_OGG_PROJECT}"
# libvorbis_static.vcxproj doesn't really produce statically linkable libraries
# (code generation settings in the project require MSVCRT). Fix that before
# building.
fix_libvorbis_vcxproj
build_libs "${MSBUILD_VORBIS_PATH}/${MSBUILD_VORBIS_PROJECT}"

# Install headers and libraries.
install_ogg_files
install_vorbis_files

# Echo the new text for README.webmdshow.
cat << EOF
Ogg and Vorbis libraries updated. README.webmdshow information follows:

libogg
Version: ${OGG_VERSION}
URL: ${XIPH_DL_SERVER}/${XIPH_DL_LIBOGG}

libvorbis
Version: ${VORBIS_VERSION}
URL: ${XIPH_DL_SERVER}/${XIPH_DL_LIBVORBIS}
EOF

vlog "Done."
