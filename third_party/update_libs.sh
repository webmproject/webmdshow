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

set -e

# TODO(tomfinegan): update script to support IDEs other than vs2008
# TODO(tomfinegan): this script needs some clean up (style nastiness, mainly).

# xiph config vars
# TODO(tomfinegan): rename some of these vars to distinguish them from
#                   the libvpx vars
build_config_array=(debug\|win32 debug\|x64 release\|win32 release\|x64)
libvorbis_url=\
"http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.3.tar.gz"
libogg_url="http://downloads.xiph.org/releases/ogg/libogg-1.3.0.tar.gz"
patches_dir="patches"
# note: the order of the following two arrays must match
devenv_outdir_array=(win32/debug x64/debug win32/release x64/release)
install_dir_array=(x86/debug x64/debug x86/release x64/release)

# libvpx stuff
libvpx_dir="libvpx.git"
libvpx_remote="http://git.chromium.org/webm/libvpx.git"
libvpx_git_target="eider"
libvpx_target_array=(x86-win32-vs9 x86_64-win64-vs9)

# TODO(tomfinegan): add proper command line parsing
if [[ "$1" == "--novpx" ]]; then
  disable_libvpx_build="disable"
elif [[ "$1" == "--noxiph" ]]; then
  disable_xiph_builds="disable"
fi

if [[ "$1" == "--novorbis" ]] || [[ "$2" == "--novorbis" ]]; then
  disable_vorbis_build="disable"
fi

function die() {
  if [[ -n "$1" ]]; then
    echo "ERROR> ($1) exiting!"
  else
    echo "ERROR> exiting!"
  fi
  exit 1
}

function download_and_untar() {
  local readonly url="$1"
  if [[ -z "$url" ]]; then
    die "${FUNCNAME[0]}: no url specified [arg1 empty]"
  fi
  local readonly outfile=$(basename ${url})

  # Try using wget or curl to grab the archive.
  # TODO(tomfinegan): eat stderr when which fails.
  if [[ -n $(which wget) ]]; then
    wget ${url}
  elif [[ -n $(which curl) ]]; then
    curl ${url} -O
  else
    die "${FUNCNAME[0]}: either wget or curl must be in \$PATH"
  fi
  if [[ ! -e "${outfile}" ]]; then
    die "${FUNCNAME[0]}: download failed [argurl]"
  fi
  local readonly outfiles=$(tar xvzf "${outfile}")

  # echo output dir
  #  - there's probably a better way to do this, but here's to fast and ugly!
  echo "${outfiles}" | tr '[:space:]' '\n' | head -n1
}

function build_xiph_lib() {
  local readonly libdir="$1"
  if [[ -z "$libdir" ]]; then
    die "${FUNCNAME[0]}: no libdir specified [arg1 empty]"
  fi
  local readonly solution="$2"
  if [[ -z "$solution" ]]; then
    die "${FUNCNAME[0]}: no solution specified [arg2 empty]"
  fi
  local readonly project="$3"
  if [[ -z "$project" ]]; then
    die "${FUNCNAME[0]}: no project specified [arg3 empty]"
  fi
  # need vs2008 in your path for this bit
  for config in ${build_config_array[@]}; do
    devenv.com ${libdir}win32/VS2008/${solution}.sln -Project ${project} \
        -Rebuild "${config}"
  done
}

function fix_xiph_project() {
  local readonly projdir="$1"
  if [[ -z "$projdir" ]]; then
    die "${FUNCNAME[0]}: no projdir specified [arg1 empty]"
  fi
  local readonly project="$2"
  if [[ -z "$project" ]]; then
    die "${FUNCNAME[0]}: no project specified [arg2 empty]"
  fi
  local readonly vcproj="${projdir}/${project}.vcproj"
  if [[ ! -e "${vcproj}" ]]; then
    die "${FUNCNAME[0]}: project file (${vcproj}) does not exist"
  fi
  sed -i \
    -e "s/RuntimeLibrary=\"3\"/RuntimeLibrary=\"1\"/g" \
    -e "s/RuntimeLibrary=\"2\"/RuntimeLibrary=\"0\"/g" \
    -e "s/DebugInformationFormat=\"4\"/DebugInformationFormat=\"3\"/g" \
    -e "s/DebugInformationFormat=\"0\"/DebugInformationFormat=\"3\"/g" \
    ${vcproj}
  local readonly ogg_dir=$(basename ${libogg_url} .tar.gz)
  if [[ "${project}" == "libvorbis_static" ]]; then
    # Fix the ogg include path in the vorbis project.
    sed -i -e "s/libogg\([\\]\+inc\)/${ogg_dir}\1/" ${vcproj}
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
    # copy pdb files.
    local pdb_ogg="${src_dir}/win32/VS2008/"
    pdb_ogg="${pdb_ogg}${devenv_outdir_array[$lib_index]}/vc90.pdb"
    if [[ -e "${pdb_ogg}" ]]; then
      cp "${pdb_ogg}" ${target_dir}/${install_dir_array[$lib_index]}/
    else
      local pdb_vorbis="${src_dir}/win32/VS2008/libvorbis/"
      pdb_vorbis="${pdb_vorbis}${devenv_outdir_array[$lib_index]}/vc90.pdb"
      cp "${pdb_vorbis}" ${target_dir}/${install_dir_array[$lib_index]}/
    fi
  done
}

function clone_libvpx_and_checkout_tag() {
  local git_quiet="-q"
  local clone_dir="$1"
  if [[ -z "$clone_dir" ]]; then
    die "clone_libvpx_and_checkout_tag: no clone_dir specified [arg1 empty]"
  fi
  local git_remote="$2"
  if [[ -z "$git_remote" ]]; then
    die "clone_libvpx_and_checkout_tag: no git_remote specified [arg2 empty]"
  fi
  local git_checkout_target="$3"
  if [[ -z "$git_checkout_target" ]]; then
    die "clone_libvpx_and_checkout_tag: no git_checkout_target specified \
        [arg3 empty]"
  fi
  # Just clone into .; the libvpx directory will be deleted later. 
  local tmp_path=.
  if [[ ! -d "${tmp_path}" ]]; then
    die "clone_libvpx_and_checkout_tag: temp dir creation failed \
         [tmp_path does not exist]"
  fi
  # clone the repo-- quietly, otherwise a line of git output lingers in
  # stdout and breaks the attempt to echo the full path to the clone.
  # Some weird issue w/sync in cygwin maybe...
  git clone ${git_quiet} "${git_remote}" "${tmp_path}/${clone_dir}"
  if [[ ! -d "${tmp_path}/${clone_dir}" ]]; then
    die "clone_libvpx_and_checkout_tag: clone failed [directory does not exist]"
  fi
  cd "${tmp_path}/${clone_dir}"
  # checkout
  git checkout ${git_quiet} "${git_checkout_target}"
  # echo the clone path so the next bit of script can do something with it
  #echo "tmp_path=$tmp_path"
  #echo "clone_dir=$clone_dir"
  sync # flush git output from stdout (this doesn't work... perhaps I'm
       # "doing it wrong")
  echo "${tmp_path}/${clone_dir}"
}

function build_libvpx() {
  
  local clone_dir="$1"
  if [[ -z "$clone_dir" ]]; then
    die "build_libvpx: clone_dir not specified [arg1 empty]"
  fi
  # already confirmed temp dir and clone exist earlier
  local olddir="$(pwd)"
  cd "${clone_dir}"
  #echo "num_entries=${#libvpx_target_array[@]}"
  #echo "entries=${libvpx_target_array[@]}"
  for config in ${libvpx_target_array[@]}; do
    #echo "config=$config"
    mkdir -p "${config}"
    cd "${config}"
    ../configure --target=${config} --disable-examples --enable-static-msvcrt \
--disable-install-docs
    make clean
    make
    # "installs" the includes within INSTALL/include/vpx, which are copied
    # to webmdshow.git/third_party/libvpx/vpx by |install_libvpx_files|
    DIST_DIR=./INSTALL make install
    cd -
  done
  cd "${olddir}"
}

function install_libvpx_files() {
  local clone_dir="$1"
  if [[ -z "$clone_dir" ]]; then
    die "install_libvpx_files: clone_dir not specified [arg1 empty]"
  fi
  local targets_array=(${libvpx_target_array[@]})
  for (( target_num=0; target_num < "${#targets_array[@]}"; target_num++ )); do
    # vs2k8 drops libs in win32/x64 subdirs of the configuration dir when
    # configured for those targets, which differs from x86/x86_64 as w/in
    # the actual libvpx target names-- just grab the first four chars in
    # current target to avoid the issue...
    local target="${targets_array[${target_num}]}"
    local lib_src_subdir="${target:0:4}"
    local lib_dest_subdir="${target:0:3}"
    # and replace w/the proper build subdir
    if [[ "${lib_src_subdir}" == "x86-" ]]; then
      lib_src_subdir="Win32"
    elif [[ "${lib_src_subdir}" == "x86_" ]]; then
      lib_src_subdir="x64"
      lib_dest_subdir="${lib_src_subdir}"
    else
      die "install_libvpx_files: unexpected entry in target array substr\
! [expected x86_ or x86-, got ${lib_src_subdir}]"
    fi
    #echo "pwd=$(pwd)"
    local lib_build_path="${clone_dir}/${target}/${lib_src_subdir}/"
    cp "${lib_build_path}Debug/vpxmtd.lib" "libvpx/${lib_dest_subdir}/debug/"
    cp "${lib_build_path}/Release/vpxmt.lib" \
"libvpx/${lib_dest_subdir}/release/"
  done

  # copy LICENSE and includes
  cp "${clone_dir}/LICENSE" "libvpx/"
  cp ${clone_dir}/${target}/INSTALL/include/vpx/*.h libvpx/vpx/
}

# Xiph stuff
if [[ -z "${disable_xiph_builds}" ]]; then
  # download/extract libogg and libvorbis distributions
  libogg_dir=$(download_and_untar "${libogg_url}")
  #echo "libogg_dir=$libogg_dir"
  libvorbis_dir=$(download_and_untar "${libvorbis_url}")
  #echo "libvorbis_dir=$libvorbis_dir"
  # Patch up the xiph projects w/a couple of fixes
  # 1. use static run times
  # 2. update ogg include path in vorbis proj (only if enabled)
  fix_xiph_project "${libogg_dir}win32/VS2008" libogg_static
  # build debug and release libs for win32 and x64 targets
  build_xiph_lib "${libogg_dir}" libogg_static libogg_static
  # install libs and includes for webmdshow/webmmediafoundation
  install_xiph_files libogg_static "${libogg_dir}" libogg ogg
  if [[ -z "${disable_vorbis_build}" ]]; then
    # patch/build libvorbis only if enabled
    fix_xiph_project "${libvorbis_dir}win32/VS2008/libvorbis" libvorbis_static
    build_xiph_lib "${libvorbis_dir}" vorbis_static libvorbis_static
    install_xiph_files libvorbis_static "${libvorbis_dir}" libvorbis vorbis
  fi
  # clean up and remove an unnecessary file.
  rm -rf "${libogg_dir}" "${libvorbis_dir}" *.tar.gz \
      libvorbis/vorbis/vorbisfile.h
fi

# libvpx stuff
if [[ -z "${disable_libvpx_build}" ]]; then
  set -e
  libvpx_full_path=$(clone_libvpx_and_checkout_tag "${libvpx_dir}" \
"${libvpx_remote}" "${libvpx_git_target}")
  # Since git refuses to obey -q under some circumstances at clone/checkout 
  # time we have to clean up the directory here: awk abuse to the rescue!
  libvpx_full_path=$(echo ${libvpx_full_path} | \
                   awk 'BEGIN {FS="[ \n]"};{print $NF}')
  build_libvpx "${libvpx_full_path}"
  install_libvpx_files "${libvpx_full_path}"
  rm -rf "${libvpx_full_path}"
fi

