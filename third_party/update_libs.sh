#!/bin/bash
# RUN THIS SCRIPT FROM third_party in webmdshow.git!
# Requirements: patch, wget, and vcbuild must be in your path.
# You must have cygwin installed, or some other package that provides
# a bash shell, like msys.
#
# To duplicate the environment this script was written in:
# 1. Start a visual studio 2008 command prompt
# 2. Run c:\cygwin\Cygwin.bat
# 3. (Optional) run mintty.exe once Cygwin.bat finishes.
#    - this provides a fully functional terminal

build_config_array=(debug\|win32 debug\|x64 release\|win32 release\|x64)
libvorbis_url=\
"http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.1.tar.gz"
libogg_url="http://downloads.xiph.org/releases/ogg/libogg-1.2.0.tar.gz"
patches_dir="patches"
# note: the order of the following two arrays must match
devenv_outdir_array=(win32/debug x64/debug win32/release x64/release)
install_dir_array=(x86/debug x64/debug x86/release x64/release)

disable_xiph_builds="" # disables ogg and vorbis builds
function die() {
  if [[ -n "$1" ]]; then
    echo "ERROR> ($1) exiting!"
  else
    echo "ERROR> exiting!"
  fi
  exit 1
}

function wget_and_untar() {
  local url="$1"
  if [[ -z "$url" ]]; then
    die "wget_and_untar: no url specified [arg1 empty]"
  fi
  local oldIFS="${IFS}"
  IFS=/
  local outfile_url_array
  read -ra outfile_url_array <<< "${url}"
  #echo "outfile_url_array=${outfile_url_array[@]}"
  IFS="${oldIFS}"
  local last_array_element=${#outfile_url_array[@]}-1
  local outfile="${outfile_url_array[arg{last_array_element}]}"
  wget "${url}"
  if [[ ! -e "${outfile}" ]]; then
    die "wget_and_untar: download failed [argurl]"
  fi
  local outfiles=$(tar xvzf "${outfile}")

  # echo output dir
  #  - there's probably a better way to do this, but here's to fast and ugly!
  echo "${outfiles}" | tr \[:space:\] \\n | head -n1
}

function build_xiph_lib() {
  local libdir="$1"
  if [[ -z "$libdir" ]]; then
    die "build_xiph_lib: no libdir specified [arg1 empty]"
  fi
  local solution="$2"
  if [[ -z "$solution" ]]; then
    die "build_xiph_lib: no solution specified [arg2 empty]"
  fi
  local project="$3"
  if [[ -z "$project" ]]; then
    die "build_xiph_lib: no project specified [arg3 empty]"
  fi
  # need vs2008 in your path for this bit
  for config in ${build_config_array[@]}; do
    devenv.com ${libdir}win32/VS2008/${solution}.sln -Project ${project} \
-Rebuild "${config}"
  done
}

function fix_xiph_project() {
  local projdir="$1"
  if [[ -z "$projdir" ]]; then
    die "fix_xiph_project: no projdir specified [arg1 empty]"
  fi
  local project="$2"
  if [[ -z "$project" ]]; then
    die "fix_xiph_project: no project specified [arg2 empty]"
  fi
  sed -i -e "s/RuntimeLibrary=\"3\"/RuntimeLibrary=\"1\"/g" \
-e "s/RuntimeLibrary=\"2\"/RuntimeLibrary=\"0\"/g" \
${projdir}/${project}.vcproj
  if [[ "${project}" == "libvorbis_static" ]]; then
    sed -i -e \
"s/libogg\\\\inc/libogg-1\.2\.0\\\\inc/g" \
${projdir}/${project}.vcproj
  fi
}

function install_xiph_files() {
  local project="$1"
  if [[ -z "$project" ]]; then
    die "install_xiph_files: no project specified [arg1 empty]"
  fi
  local src_dir="$2"
  if [[ -z "$src_dir" ]]; then
    die "install_xiph_files: no src_dir specified [arg2 empty]"
  fi
  local target_dir="$3"
  if [[ -z "$target_dir" ]]; then
    die "install_xiph_files: no target_dir specified [arg3 empty]"
  fi
  local inc_dir="$4"
  if [[ -z "$inc_dir" ]]; then
    die "install_xiph_files: no inc_dir specified [arg4 empty]"
  fi

  # remove old includes
  rm ${target_dir}/${inc_dir}/*.h
  # and old libs
  for libpath in ${install_dir_array[@]}; do
    rm ${target_dir}/${libpath}/${project}.lib
  done

  # copy in the new includes
  cp ${src_dir}/include/${inc_dir}/*.h ${target_dir}/${inc_dir}/
  # and the new libs
  for (( lib_index=0; lib_index < ${#install_dir_array[@]}; lib_index++ )); do
    cp ${src_dir}/win32/VS2008/${devenv_outdir_array[$lib_index]}/${project}.lib \
${target_dir}/${install_dir_array[$lib_index]}/
  done
}




# Xiph stuff
if [[ -z "${disable_xiph_builds}" ]]; then
  # download/extract libogg and libvorbis distributions
  libogg_dir=$(wget_and_untar "${libogg_url}")
  #echo "libogg_dir=$libogg_dir"
  libvorbis_dir=$(wget_and_untar "${libvorbis_url}")
  #echo "libvorbis_dir=$libvorbis_dir"
  # Patch up the xiph projects w/a couple of fixes
  # 1. use static run times
  # 2. update ogg include path in vorbis proj
  fix_xiph_project "${libogg_dir}win32/VS2008" libogg_static
  fix_xiph_project "${libvorbis_dir}win32/VS2008/libvorbis" libvorbis_static
  # build debug and release libs for win32 and x64 targets
  build_xiph_lib "${libogg_dir}" libogg_static libogg_static
  build_xiph_lib "${libvorbis_dir}" vorbis_static libvorbis_static
  # install libs and includes for webmdshow/webmmediafoundation
  install_xiph_files libogg_static "${libogg_dir}" libogg ogg
  install_xiph_files libvorbis_static "${libvorbis_dir}" libvorbis vorbis
  # clean up and remove a couple of unnecessary files
  rm -rf "${libogg_dir}" "${libvorbis_dir}" *.tar.gz libvorbis/vorbis/vorbis*.h
fi


