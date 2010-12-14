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

  !include "MUI2.nsh"
  !include "x64.nsh"
  !define MUI_COMPONENTSPAGE_NODESC
  !define MUI_ICON "..\..\webm.ico"
  !define MUI_UNICON "..\..\webm.ico"

;--------------------------------
;General

  ;Name and file
  Name "WebM Media Foundation"
  OutFile "install_webmmf.exe"

  ; enable next line if we must
  RequestExecutionLevel "admin"

  ; WMMF_UNINSTALL_KEY is where information is stored for Add/Remove Programs
  !define WMMF_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\webmmf"

  ; Set compression type for the installer data.
  SetCompressor /SOLID lzma

  ShowInstDetails show
  ShowUnInstDetails show

  ; Version Information
  ;;http://nsis.sourceforge.net/Docs/Chapter4.html#4.8.3
  ;;http://stackoverflow.com/questions/4244497/changing-nsis-installer-properties

  ; Info from MS about ProductVersion Property
  ;;http://msdn.microsoft.com/en-us/library/aa370859.aspx

  ;VIProductVersion $0
  ;VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "WebM Media Foundation Components"
  ;VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Webm Project"
  ;VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" $0
  ;VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" $0
  ;VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Installer for Webm Media Foundation components"

;--------------------------------
;Interface Settings

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  ;!insertmacro MUI_PAGE_LICENSE "${NSISDIR}\Docs\Modern UI\License.txt"
  ;!insertmacro MUI_PAGE_COMPONENTS
  ;!insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  ;!insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Install" SecInstall

  SetAutoClose false

  ; Set install directory for x86 (32-bit) DLLs
  SetOutPath "$COMMONFILES32\WebM Project\webmmf"

  File "..\..\..\dll\webmmf\Win32\Release\webmmfsource32.dll"
  File "..\..\..\dll\webmmf\Win32\Release\webmmfvorbisdec32.dll"
  File "..\..\..\dll\webmmf\Win32\Release\webmmfvp8dec32.dll"

  ${If} ${RunningX64}

    ; Set install directory for x64 DLLs
    SetOutPath "$COMMONFILES64\WebM Project\webmmf"

    File "..\..\..\dll\webmmf\x64\Release\webmmfsource64.dll"
    File "..\..\..\dll\webmmf\x64\Release\webmmfvorbisdec64.dll"
    File "..\..\..\dll\webmmf\x64\Release\webmmfvp8dec64.dll"

  ${EndIf}

  WriteUninstaller "$OUTDIR\uninstall_webmmf.exe"

  ; Create Add/Remove programs keys
  ;;http://nsis.sourceforge.net/Add_uninstall_information_to_Add/Remove_Programs

  ; Description of GetDllVersion
  ;;http://nsis.sourceforge.net/GetDllVersion_Command_Explained

  ${If} ${RunningX64}
     StrCpy $0 "$COMMONFILES64\WebM Project\webmmf\webmmfsource64.dll"
  ${Else}
     StrCpy $0 "$COMMONFILES32\WebM Project\webmmf\webmmfsource32.dll"
  ${EndIf}

  GetDllVersion $0 $R0 $R1  ; $R0 = major | minor  $R1 = release | build

  IntOp $R2 $R0 >> 16
  IntOp $R2 $R2 & 0x0000FFFF  ; $R2 = major
  IntOp $R3 $R0 & 0x0000FFFF  ; $R3 = minor
  IntOp $R4 $R1 >> 16
  IntOp $R4 $R4 & 0x0000FFFF  ; $R4 = release
  IntOp $R5 $R1 & 0x0000FFFF  ; $R5 = build

  WriteRegDWORD HKCU "${WMMF_UNINSTALL_KEY}" "VersionMajor" $R2
  WriteRegDWORD HKCU "${WMMF_UNINSTALL_KEY}" "VersionMinor" $R3

  StrCpy $0 "$R2.$R3.$R4.$R5"
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "DisplayVersion" $0

  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "DisplayName" "WebM Media Foundation Components"
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "UninstallString" "$OUTDIR\uninstall_webmmf.exe"
  WriteRegDWORD HKCU "${WMMF_UNINSTALL_KEY}" "NoModify" 0x00000001
  WriteRegDWORD HKCU "${WMMF_UNINSTALL_KEY}" "NoRepair" 0x00000001
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "Publisher" "WebM Project"
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "URLInfoAbout" "http://www.webmproject.org"
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "Comments" "WebM Media Foundation COM-server DLLs"
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "InstallLocation" "$OUTDIR"
  WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "InstallSource" "http://code.google.com/p/webm/downloads/list"
  ;WriteRegStr HKCU "${WMMF_UNINSTALL_KEY}" "ModifyPath" "$OUTDIR\uninstall_webmmf.exe"

  SetOutPath "$COMMONFILES32\WebM Project\webmmf"

  ClearErrors
  RegDLL "$OUTDIR\webmmfsource32.dll"

  IfErrors 0 +2
     MessageBox MB_OK "webmmfsource32.dll failed to register."

  ClearErrors
  RegDLL "$OUTDIR\webmmfvorbisdec32.dll"

  IfErrors 0 +2
     MessageBox MB_OK "webmmfvorbisdec32.dll failed to register."

  ClearErrors
  RegDLL "$OUTDIR\webmmfvp8dec32.dll"

  IfErrors 0 +2
     MessageBox MB_OK "webmmfvp8dec32.dll failed to register."

  ${If} ${RunningX64}

    ;;RegDLL doesn't seem to work for x64 DLLs, so we used
    ;;work-around described here:
    ;;http://forums.winamp.com/showthread.php?t=297168

    SetOutPath "$COMMONFILES64\WebM Project\webmmf"

    ClearErrors
    ;RegDLL "$OUTDIR\webmmfsource64.dll"
    ExecWait '"$SYSDIR\regsvr32.exe" /s "$OUTDIR\webmmfsource64.dll"'

    IfErrors 0 +2
       MessageBox MB_OK "webmmfsource64.dll failed to register."

    ClearErrors
    ;RegDLL "$OUTDIR\webmmfvorbisdec64.dll"
    ExecWait '"$SYSDIR\regsvr32.exe" /s "$OUTDIR\webmmfvorbisdec64.dll"'

    IfErrors 0 +2
       MessageBox MB_OK "webmmfvorbisdec64.dll failed to register."

    ClearErrors
    ;RegDLL "$OUTDIR\webmmfvp8dec64.dll"
    ExecWait '"$SYSDIR\regsvr32.exe" /s "$OUTDIR\webmmfvp8dec64.dll"'

    IfErrors 0 +2
       MessageBox MB_OK "webmmfvp8dec64.dll failed to register."

  ${EndIf}

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall" SecUninstall

  SetAutoClose false

  SetOutPath "$COMMONFILES32\WebM Project\webmmf"

  ClearErrors
  UnRegDLL "$OUTDIR\webmmfsource32.dll"

  IfErrors 0 +2
    MessageBox MB_OK "webmmfsource32.dll failed to unregister."

  Delete /REBOOTOK "$OUTDIR\webmmfsource32.dll"

  ClearErrors
  UnRegDLL "$OUTDIR\webmmfvorbisdec32.dll"

  IfErrors 0 +2
    MessageBox MB_OK "webmmfvorbisdec32.dll failed to unregister."

  Delete /REBOOTOK "$OUTDIR\webmmfvorbisdec32.dll"

  ClearErrors
  UnRegDLL "$OUTDIR\webmmfvp8dec32.dll"

  IfErrors 0 +2
    MessageBox MB_OK "webmmfvp8dec32.dll failed to unregister."

  Delete /REBOOTOK "$OUTDIR\webmmfvp8dec32.dll"

  ${If} ${RunningX64}

    SetOutPath "$COMMONFILES64\WebM Project\webmmf"

    ClearErrors
    ;UnRegDLL "$OUTDIR\webmmfsource64.dll"
    ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$OUTDIR\webmmfsource64.dll"'

    IfErrors 0 +2
      MessageBox MB_OK "webmmfsource64.dll failed to unregister."

    Delete /REBOOTOK "$OUTDIR\webmmfsource64.dll"

    ClearErrors
    ;UnRegDLL "$OUTDIR\webmmfvorbisdec64.dll"
    ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$OUTDIR\webmmfvorbisdec64.dll"'

    IfErrors 0 +2
      MessageBox MB_OK "webmmfvorbisdec64.dll failed to unregister."

    Delete /REBOOTOK "$OUTDIR\webmmfvorbisdec64.dll"

    ClearErrors
    ;UnRegDLL "$OUTDIR\webmmfvp8dec64.dll"
    ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$OUTDIR\webmmfvp8dec64.dll"'

    IfErrors 0 +2
      MessageBox MB_OK "webmmfvp8dec64.dll failed to unregister."

    Delete /REBOOTOK "$OUTDIR\webmmfvp8dec64.dll"

  ${EndIf}

  ;TODO: use $INSTDIR and installDir for to handle this

  ${If} ${RunningX64}

    Delete "$COMMONFILES64\Webm Project\webmmf\uninstall_webmmf.exe"
    RMDir /r "$COMMONFILES64\Webm Project\webmmf"

  ${Else}

    Delete "$COMMONFILES32\Webm Project\webmmf\uninstall_webmmf.exe"

  ${EndIf}

  RMDir /r "$COMMONFILES32\Webm Project\webmmf"

  DeleteRegKey HKCU "${WMMF_UNINSTALL_KEY}"

SectionEnd
