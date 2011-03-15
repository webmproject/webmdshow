;
; Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
; Use of this source code is governed by a BSD-style license and patent
; grant that can be found in the LICENSE file in the root of the source
; tree. All contributing project authors may be found in the AUTHORS
; file in the root of the source tree.
;

; WebM Media Foundation components installer.

  !include "FileAssociation.nsh"
  !include "Library.nsh"
  !include "MUI2.nsh"
  !include "webmmf_util.nsh"
  !include "x64.nsh"

  !define MUI_COMPONENTSPAGE_NODESC
  !define MUI_ICON "..\..\webm.ico"
  !define MUI_UNICON "..\..\webm.ico"

;--------------------------------
;General

  ;Name and file
  Name "WebM Media Foundation"
  OutFile "install_webmmf.exe"

  ; COM and Media Foundation registration require admin privileges.
  RequestExecutionLevel "admin"

  ; WMMF_UNINSTALL_KEY is where information is stored for Add/Remove Programs
  !define WMMF_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\webmmf"

  ; Set compression type for the installer data.
  SetCompressor /SOLID lzma

  ShowInstDetails show
  ShowUnInstDetails show

  Var VERSION_STR

  ; Helper defines used during installation
  ; WMF Source 32-bit DLL name
  !define WMMF_SOURCE_DLL32 "webmmfsource32.dll"
  ; WMF Source 64-bit DLL name
  !define WMMF_SOURCE_DLL64 "webmmfsource64.dll"
  ; WMF Vorbis decoder 32-bit DLL name
  !define WMMF_VORBIS_DLL32 "webmmfvorbisdec32.dll"
  ; WMF Vorbis decoder 64-bit DLL name
  !define WMMF_VORBIS_DLL64 "webmmfvorbisdec64.dll"
  ; WMF VP8 decoder 32-bit DLL name
  !define WMMF_VP8_DLL32 "webmmfvp8dec32.dll"
  ; WMF VP8 decoder 64-bit DLL name
  !define WMMF_VP8_DLL64 "webmmfvp8dec64.dll"
  ; Path to 32-bit DLLs on NSIS compiler system
  !define WMMF_BUILD_PATH32 "..\..\..\dll\webmmf\Win32\Release"
  ; Path to 64-bit DLLs on NSIS compiler system
  !define WMMF_BUILD_PATH64 "..\..\..\dll\webmmf\x64\Release"
  ; Install path on 32-bit systems
  !define WMMF_INSTALL_PATH32 "$COMMONFILES32\WebM Project\webmmf"
  ; Install path on 64-bit systems
  !define WMMF_INSTALL_PATH64 "$COMMONFILES64\WebM Project\webmmf"
  ; Error message suffixes used with CheckError macro
  !define WMMF_INSTALL_ERROR "installation failed."
  !define WMMF_UNINSTALL_ERROR "uninstallation failed."
  ; Omaha (Google Update) defines
  !define WMMF_OMAHA_CLIENT_SUBKEY "Software\Google\Update\Clients"
  !define WMMF_OMAHA_RESULT_SUBKEY "Software\Google\Update\ClientState"
  !define WMMF_OMAHA_GUID "{ED3112D0-5211-11DF-94AF-0026B977EEAA}"
  !define WMMF_OMAHA_APPNAME "WebM Media Foundation Components"
  !define WMMF_OMAHA_REBOOT_REQUIRED 3010
  !define WMMF_OMAHA_SUCCESS 0
  ; File association related...
  !define WMMF_FILE_EXT ".webm"
  !define WMMF_FILE_EXT_DESCRIPTION "WebM Media File"
  !define WMP_EXTENSIONS_HKLM_SUBKEY \
"Software\Microsoft\Multimedia\WMPlayer\Extensions\.webm"
  !define WMP_PATH "$PROGRAMFILES\Windows Media Player\wmplayer.exe"
;--------------------------------
;Interface Settings

;--------------------------------
;Pages

  ; installer pages
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  ; uninstaller pages
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Install" SecInstall

  ; Allow user to inspect intaller DetailPrint output
  SetAutoClose false

  ${If} ${RunningX64}
    ; StrCpy $0 "${WMMF_BUILD_PATH64}\${WMMF_SOURCE_DLL64}"
    GetDllVersionLocal "${WMMF_BUILD_PATH64}\${WMMF_SOURCE_DLL64}" $R0 $R1
  ${Else}
    ; StrCpy $0 "${WMMF_BUILD_PATH32}\${WMMF_SOURCE_DLL32}"
    GetDllVersionLocal "${WMMF_BUILD_PATH32}\${WMMF_SOURCE_DLL32}" $R0 $R1
  ${EndIf}

  ; GetDllVersionLocal stored the version dwords in R0 and R1
  ; $R0 = major | minor  $R1 = release | build
  IntOp $R2 $R0 >> 16
  IntOp $R2 $R2 & 0x0000FFFF  ; $R2 = major
  IntOp $R3 $R0 & 0x0000FFFF  ; $R3 = minor
  IntOp $R4 $R1 >> 16
  IntOp $R4 $R4 & 0x0000FFFF  ; $R4 = release
  IntOp $R5 $R1 & 0x0000FFFF  ; $R5 = build

  ; Create Add/Remove programs keys
  ;;http://nsis.sourceforge.net/Add_uninstall_information_to_Add/Remove_Programs
  WriteRegDWORD HKLM "${WMMF_UNINSTALL_KEY}" "VersionMajor" $R2
  WriteRegDWORD HKLM "${WMMF_UNINSTALL_KEY}" "VersionMinor" $R3

  StrCpy $VERSION_STR "$R2.$R3.$R4.$R5"
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "DisplayVersion" $VERSION_STR

  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "DisplayName" \
"WebM Media Foundation Components"
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "UninstallString" \
"${WMMF_INSTALL_PATH32}\uninstall_webmmf.exe"
  WriteRegDWORD HKLM "${WMMF_UNINSTALL_KEY}" "NoModify" 0x00000001
  WriteRegDWORD HKLM "${WMMF_UNINSTALL_KEY}" "NoRepair" 0x00000001
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "Publisher" "WebM Project"
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "URLInfoAbout" \
"http://www.webmproject.org"
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "Comments" \
"WebM Media Foundation COM-server DLLs"
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "InstallLocation" \
"${WMMF_INSTALL_PATH32}"
  WriteRegStr HKLM "${WMMF_UNINSTALL_KEY}" "InstallSource" \
"http://code.google.com/p/webm/downloads/list"


  WriteRegStr HKLM "${WMMF_OMAHA_CLIENT_SUBKEY}\${WMMF_OMAHA_GUID}" \
"pv" $VERSION_STR
  WriteRegStr HKLM "${WMMF_OMAHA_CLIENT_SUBKEY}\${WMMF_OMAHA_GUID}" \
"name" "${WMMF_OMAHA_APPNAME}"
  WriteRegStr HKLM "${WMMF_OMAHA_CLIENT_SUBKEY}\${WMMF_OMAHA_GUID}" \
"lang" "en"

  ClearErrors

  ; 32-bit component install (always installed)
  ;
  ; Set install directory/write uninstaller
  SetOutPath "${WMMF_INSTALL_PATH32}"
  WriteUninstaller "${WMMF_INSTALL_PATH32}\uninstall_webmmf.exe"

  ; define LIBRARY_COM per NSIS docs when using InstallLib
  ; to register COM DLLs (see Appendix B)
  !define LIBRARY_COM

  ; abuse InstallLib to handle any locked file issues etc
  ; note that this handles use the File and RegDLL steps for us
  !insertmacro InstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_BUILD_PATH32}\${WMMF_SOURCE_DLL32}" \
"${WMMF_INSTALL_PATH32}\${WMMF_SOURCE_DLL32}" \
"${WMMF_INSTALL_PATH32}\upgrade_temp\${WMMF_SOURCE_DLL32}"
  !insertmacro CheckError "${WMMF_SOURCE_DLL32} ${WMMF_INSTALL_ERROR}"

  !insertmacro InstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_BUILD_PATH32}\${WMMF_VORBIS_DLL32}" \
"${WMMF_INSTALL_PATH32}\${WMMF_VORBIS_DLL32}" \
"${WMMF_INSTALL_PATH32}\upgrade_temp\${WMMF_VORBIS_DLL32}"
  !insertmacro CheckError "${WMMF_VORBIS_DLL32} ${WMMF_INSTALL_ERROR}"

  !insertmacro InstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_BUILD_PATH32}\${WMMF_VP8_DLL32}" \
"${WMMF_INSTALL_PATH32}\${WMMF_VP8_DLL32}" \
"${WMMF_INSTALL_PATH32}\upgrade_temp\${WMMF_VP8_DLL32}"
  !insertmacro CheckError "${WMMF_VP8_DLL32} ${WMMF_INSTALL_ERROR}"

  ;
  ; install the 64-bit dll's only on 64-bit systems
  ${If} ${RunningX64}
    ;
    ; Set install directory for x64 DLLs
    SetOutPath "${WMMF_INSTALL_PATH64}"

    ; must define LIBRARY_X64 for 64-bit COM dlls
    !define LIBRARY_X64

    !insertmacro InstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_BUILD_PATH64}\${WMMF_SOURCE_DLL64}" \
"${WMMF_INSTALL_PATH64}\${WMMF_SOURCE_DLL64}" \
"${WMMF_INSTALL_PATH64}\upgrade_temp\${WMMF_SOURCE_DLL64}"
    !insertmacro CheckError "${WMMF_SOURCE_DLL64} ${WMMF_INSTALL_ERROR}"

    !insertmacro InstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_BUILD_PATH64}\${WMMF_VORBIS_DLL64}" \
"${WMMF_INSTALL_PATH64}\${WMMF_VORBIS_DLL64}" \
"${WMMF_INSTALL_PATH64}\upgrade_temp\${WMMF_VORBIS_DLL64}"
    !insertmacro CheckError "${WMMF_VORBIS_DLL64} ${WMMF_INSTALL_ERROR}"

    !insertmacro InstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_BUILD_PATH64}\${WMMF_VP8_DLL64}" \
"${WMMF_INSTALL_PATH64}\${WMMF_VP8_DLL64}" \
"${WMMF_INSTALL_PATH64}\upgrade_temp\${WMMF_VP8_DLL64}"
    !insertmacro CheckError "${WMMF_VP8_DLL64} ${WMMF_INSTALL_ERROR}"

    !undef LIBRARY_X64

  ${EndIf}

  ; Associate WebM files with Windows Media Player
  ${RegisterExtension} "${WMP_PATH}" "${WMMF_FILE_EXT}" \
"${WMMF_FILE_EXT_DESCRIPTION}"
  ; And let WMP know it's OK to open them...
  WriteRegDWORD HKLM "${WMP_EXTENSIONS_HKLM_SUBKEY}" "Permissions" 0x00000001
  WriteRegDWORD HKLM "${WMP_EXTENSIONS_HKLM_SUBKEY}" "Runtime" 0x00000001

  # Tell Omaha we need a reboot to complete update/install
  IfRebootFlag 0 +3
    WriteRegDWORD HKLM "${WMMF_OMAHA_RESULT_SUBKEY}\${WMMF_OMAHA_GUID}" \
"InstallerResult" ${WMMF_OMAHA_REBOOT_REQUIRED}
    goto done
  ; TODO(tomfinegan): update |CheckError| to set an error flag that we
  ;                   can check here instead of blindly reporting success
  WriteRegDWORD HKLM "${WMMF_OMAHA_RESULT_SUBKEY}\${WMMF_OMAHA_GUID}" \
"InstallerResult" ${WMMF_OMAHA_SUCCESS}
done:

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall" SecUninstall

  ; Allow user to inspect intaller DetailPrint output
  SetAutoClose false

  ; 32-bit component uninstall (always removed)
  ;
  ; Set install directory
  SetOutPath "${WMMF_INSTALL_PATH32}"

  ; abuse InstallLib to handle any locked file issues etc
  ; note that this handles use the File and RegDLL steps for us
  !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_INSTALL_PATH32}\${WMMF_SOURCE_DLL32}"
  !insertmacro CheckError "${WMMF_SOURCE_DLL32} ${WMMF_UNINSTALL_ERROR}"

  !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_INSTALL_PATH32}\${WMMF_VORBIS_DLL32}"
  !insertmacro CheckError "${WMMF_VORBIS_DLL32} ${WMMF_UNINSTALL_ERROR}"

  !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_INSTALL_PATH32}\${WMMF_VP8_DLL32}"
  !insertmacro CheckError "${WMMF_VP8_DLL32} ${WMMF_UNINSTALL_ERROR}"

  Delete /REBOOTOK "${WMMF_INSTALL_PATH32}"
  RMDir /r /REBOOTOK "${WMMF_INSTALL_PATH32}"
  ClearErrors

  ;
  ; uninstall the 64-bit dll's only on 64-bit systems
  ${If} ${RunningX64}
    ;
    ; Set install directory for x64 DLLs
    SetOutPath "${WMMF_INSTALL_PATH64}"

    !define LIBRARY_X64

    !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_INSTALL_PATH64}\${WMMF_SOURCE_DLL64}"
    !insertmacro CheckError "${WMMF_SOURCE_DLL64} ${WMMF_UNINSTALL_ERROR}"

    !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_INSTALL_PATH64}\${WMMF_VORBIS_DLL64}"
    !insertmacro CheckError "${WMMF_VORBIS_DLL64} ${WMMF_UNINSTALL_ERROR}"

    !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_NOTPROTECTED \
"${WMMF_INSTALL_PATH64}\${WMMF_VP8_DLL64}"
    !insertmacro CheckError "${WMMF_VP8_DLL64} ${WMMF_UNINSTALL_ERROR}"

    !undef LIBRARY_X64

    Delete /REBOOTOK "${WMMF_INSTALL_PATH64}\uninstall_webmmf.exe"
    RMDir /r /REBOOTOK "${WMMF_INSTALL_PATH64}"

  ${EndIf}

  ; Delete WebM Windows Media Player file association
  ${UnRegisterExtension} "${WMMF_FILE_EXT}" "${WMMF_FILE_EXT_DESCRIPTION}"
  ; TODO(tomfinegan): Seems a bit heavy handed to delete the extensions
  ;                   subkey for .webm in WMP land... otoh it might be
  ;                   better to err on the side of keeping the registry
  ;                   clean.
  DeleteRegKey HKLM "${WMP_EXTENSIONS_HKLM_SUBKEY}"

  DeleteRegKey HKLM "${WMMF_UNINSTALL_KEY}"
  DeleteRegKey HKLM "${WMMF_OMAHA_CLIENT_SUBKEY}\${WMMF_OMAHA_GUID}"
  DeleteRegKey HKCU "${WMMF_OMAHA_RESULT_SUBKEY}\${WMMF_OMAHA_GUID}"
  DeleteRegKey HKLM "${WMMF_OMAHA_RESULT_SUBKEY}\${WMMF_OMAHA_GUID}"
SectionEnd

