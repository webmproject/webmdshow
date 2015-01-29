;
; Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
; Use of this source code is governed by a BSD-style license and patent
; grant that can be found in the LICENSE file in the root of the source
; tree. All contributing project authors may be found in the AUTHORS
; file in the root of the source tree.
;

;WebM DShow Filter installer. Based on Modern-UI WelcomeFinish script example.

  !include "MUI2.nsh"
  !define MUI_COMPONENTSPAGE_NODESC
  !define MUI_ICON "..\webm.ico"
  !define MUI_UNICON "..\webm.ico"

;--------------------------------
;General

  ;Name and file
  Name "WebM Project Directshow Filters"
  OutFile "install_webmdshow.exe"

  ; enable next line if we must
  RequestExecutionLevel "admin"

  ; WMDS_UNINSTALL_KEY is where information is stored for Add/Remove Programs
  !define WMDS_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\webmdshow"

  ; Path to DLLs on NSIS compiler system (relative to script)
  !define WMDS_BUILD_PATH "..\..\dll\webmdshow\Release"

  ; WebM Source Filter DLL name
  !define WMDS_SOURCE_DLL "webmsource.dll"

  ; Set compression type for the installer data.
  SetCompressor /SOLID lzma

  ShowInstDetails show
  ShowUnInstDetails show

  Var VERSION_STR

;--------------------------------
;Interface Settings

;--------------------------------
;Pages

  ; Make it possible to actually read the install/uninstall logs.
  !define MUI_FINISHPAGE_NOAUTOCLOSE
  !define MUI_UNFINISHPAGE_NOAUTOCLOSE

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

  ; Set install directory
  SetOutPath "$COMMONFILES\WebM Project\webmdshow"

  ; Globals for the DLL file names; we use these in the uninstall section, so
  ; we probably want globals anyway.
  Var /GLOBAL vp8decoder_dll
  StrCpy $vp8decoder_dll "vp8decoder.dll"
  Var /GLOBAL vp8encoder_dll
  StrCpy $vp8encoder_dll "vp8encoder.dll"
  Var /GLOBAL vp9decoder_dll
  StrCpy $vp9decoder_dll "vp9decoder.dll"
  Var /GLOBAL vpxdecoder_dll
  StrCpy $vpxdecoder_dll "vpxdecoder.dll"
  Var /GLOBAL webmmux_dll
  StrCpy $webmmux_dll "webmmux.dll"
  Var /GLOBAL webmsource_dll
  StrCpy $webmsource_dll "webmsource.dll"
  Var /GLOBAL webmsplit_dll
  StrCpy $webmsplit_dll "webmsplit.dll"
  Var /GLOBAL webmcc_dll
  StrCpy $webmcc_dll "webmcc.dll"
  Var /GLOBAL webmvorbisdecoder_dll
  StrCpy $webmvorbisdecoder_dll "webmvorbisdecoder.dll"
  Var /GLOBAL webmvorbisencoder_dll
  StrCpy $webmvorbisencoder_dll "webmvorbisencoder.dll"

  ; Tell NSIS to include the filter DLL files-- via wildcard because the File
  ; instruction doesn't support use of variables!
  File "..\..\dll\webmdshow\Release\*.dll"

  ; Create uninstaller
  WriteUninstaller "$OUTDIR\uninstall_webmdshow.exe"

  ; Extract version from the WebM source DLL
  GetDllVersionLocal "${WMDS_BUILD_PATH}\${WMDS_SOURCE_DLL}" $R0 $R1

  ; GetDllVersionLocal stored the version dwords in R0 and R1
  ; $R0 = major | minor  $R1 = release | build
  IntOp $R2 $R0 >> 16
  IntOp $R2 $R2 & 0x0000FFFF  ; $R2 = major
  IntOp $R3 $R0 & 0x0000FFFF  ; $R3 = minor
  IntOp $R4 $R1 >> 16
  IntOp $R4 $R4 & 0x0000FFFF  ; $R4 = release
  IntOp $R5 $R1 & 0x0000FFFF  ; $R5 = build

  ; Write Add/Remove programs keys
  WriteRegDWORD HKCU "${WMDS_UNINSTALL_KEY}" "VersionMajor" $R2
  WriteRegDWORD HKCU "${WMDS_UNINSTALL_KEY}" "VersionMinor" $R3

  ; Copy version to a string and write it to the uninstall key
  StrCpy $VERSION_STR "$R2.$R3.$R4.$R5"
  WriteRegStr HKCU "${WMDS_UNINSTALL_KEY}" "DisplayVersion" $VERSION_STR

  WriteRegStr HKCU "${WMDS_UNINSTALL_KEY}" "DisplayName" "WebM Project Directshow Filters"
  WriteRegStr HKCU "${WMDS_UNINSTALL_KEY}" "UninstallString" "$OUTDIR\uninstall_webmdshow.exe"
  WriteRegStr HKCU "${WMDS_UNINSTALL_KEY}" "Publisher" "WebM Project"
  WriteRegDWORD HKCU "${WMDS_UNINSTALL_KEY}" "NoModify" 0x00000001
  WriteRegDWORD HKCU "${WMDS_UNINSTALL_KEY}" "NoRepair" 0x00000001

  ; Register the filter DLLs (via regsvr32)
  ClearErrors
  RegDLL "$OUTDIR\$vpxdecoder_dll"
  RegDLL "$OUTDIR\$vp8decoder_dll"
  RegDLL "$OUTDIR\$vp8encoder_dll"
  RegDLL "$OUTDIR\$vp9decoder_dll"
  RegDLL "$OUTDIR\$webmmux_dll"
  RegDLL "$OUTDIR\$webmsource_dll"
  RegDLL "$OUTDIR\$webmsplit_dll"
  RegDLL "$OUTDIR\$webmcc_dll"
  RegDLL "$OUTDIR\$webmvorbisdecoder_dll"
  RegDLL "$OUTDIR\$webmvorbisencoder_dll"

  IfErrors RegError Success

  RegError:
  ; some diagnostics
  DetailPrint "Filter DLL registration failed!"
  DetailPrint "OUTDIR: $OUTDIR"
  DetailPrint "vp8decoder_dll: $vp8decoder_dll"
  DetailPrint "vp8encoder_dll: $vp8encoder_dll"
  DetailPrint "vp9decoder_dll: $vp9decoder_dll"
  DetailPrint "vpxdecoder_dll: $vpxdecoder_dll"
  DetailPrint "webmmux_dll: $webmmux_dll"
  DetailPrint "webmsource_dll: $webmsource_dll"
  DetailPrint "webmsplit_dll: $webmsplit_dll"
  DetailPrint "webmcc_dll: $webmcc_dll"
  DetailPrint "webmvorbisdecoder_dll: $webmvorbisdecoder_dll"
  DetailPrint "webmvorbisencoder_dll: $webmvorbisencoder_dll"

  Success:

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall" SecUninstall

  ; Note: declarations are global, values are not...
  StrCpy $vp8decoder_dll "vp8decoder.dll"
  StrCpy $vp8encoder_dll "vp8encoder.dll"
  StrCpy $vp9decoder_dll "vp9decoder.dll"
  StrCpy $vpxdecoder_dll "vpxdecoder.dll"
  StrCpy $webmmux_dll "webmmux.dll"
  StrCpy $webmsource_dll "webmsource.dll"
  StrCpy $webmsplit_dll "webmsplit.dll"
  StrCpy $webmcc_dll "webmcc.dll"
  StrCpy $webmvorbisdecoder_dll "webmvorbisdecoder.dll"
  StrCpy $webmvorbisencoder_dll "webmvorbisencoder.dll"

  UnRegDLL "$INSTDIR\$vp8decoder_dll"
  UnRegDLL "$INSTDIR\$vp8encoder_dll"
  UnRegDLL "$INSTDIR\$vp9decoder_dll"
  UnRegDLL "$INSTDIR\$vpxdecoder_dll"
  UnRegDLL "$INSTDIR\$webmmux_dll"
  UnRegDLL "$INSTDIR\$webmsource_dll"
  UnRegDLL "$INSTDIR\$webmsplit_dll"
  UnRegDLL "$INSTDIR\$webmcc_dll"
  UnRegDLL "$INSTDIR\$webmvorbisdecoder_dll"
  UnRegDLL "$INSTDIR\$webmvorbisencoder_dll"

  RMDir /r /REBOOTOK $INSTDIR

  DeleteRegKey HKCU "${WMDS_UNINSTALL_KEY}"

SectionEnd
