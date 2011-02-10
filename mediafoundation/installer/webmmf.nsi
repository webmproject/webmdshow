;
; Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
; Use of this source code is governed by a BSD-style license and patent
; grant that can be found in the LICENSE file in the root of the source
; tree. All contributing project authors may be found in the AUTHORS
; file in the root of the source tree.
;

;http://nsis.sourceforge.net/Simple_tutorials
;http://www.fredshack.com/docs/nsis.html
;http://nsis.sourceforge.net/Docs/Contents.html

;WebM Media Foundation components installer.

  !include "Library.nsh"
  !include "MUI2.nsh"
  !include "x64.nsh"
  !include "webmmf_util.nsh"
  !define MUI_COMPONENTSPAGE_NODESC
  !define MUI_ICON "..\..\webm.ico"
  !define MUI_UNICON "..\..\webm.ico"

;--------------------------------
;General

  ;Name and file
  Name "WebM Media Foundation"
  OutFile "install_webmmf.exe"

  ; we need to register COM dll's: admin required
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

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall" SecUninstall

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

  DeleteRegKey HKLM "${WMMF_UNINSTALL_KEY}"

SectionEnd

