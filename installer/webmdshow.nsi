;
; Copyright (c) 2010 The WebM project authors. All Rights Reserved.
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

  ; Set compression type for the installer data.
  SetCompressor /SOLID lzma

  ShowInstDetails show
  ShowUnInstDetails show

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

  ; Set install directory
  SetOutPath "$COMMONFILES\WebM Project\webmdshow"

  ; Globals for the DLL file names; we use these in the uninstall section, so
  ; we probably want globals anyway.
  Var /GLOBAL vp8decoder_dll
  StrCpy $vp8decoder_dll "vp8decoder.dll"
  Var /GLOBAL vp8encoder_dll
  StrCpy $vp8encoder_dll "vp8encoder.dll"
  Var /GLOBAL webmmux_dll
  StrCpy $webmmux_dll "webmmux.dll"
  Var /GLOBAL webmsource_dll
  StrCpy $webmsource_dll "webmsource.dll"
  Var /GLOBAL webmsplit_dll
  StrCpy $webmsplit_dll "webmsplit.dll"

  ; Tell NSIS to include the filter DLL files-- via wildcard because the File
  ; instruction doesn't support use of variables!
  File "..\..\dll\webmdshow\Release\*.dll"

  ; Create uninstaller
  WriteUninstaller "$OUTDIR\uninstall_webmdshow.exe"

  ; Create Add/Remove programs keys
  WriteRegStr HKCU "${WMDS_UNINSTALL_KEY}" "DisplayName" "WebM Project Directshow Filters"
  WriteRegStr HKCU "${WMDS_UNINSTALL_KEY}" "UninstallString" "$OUTDIR\uninstall_webmdshow.exe"
  WriteRegDWORD HKCU "${WMDS_UNINSTALL_KEY}" "NoModify" 0x00000001
  WriteRegDWORD HKCU "${WMDS_UNINSTALL_KEY}" "NoRepair" 0x00000001

  ; Register the filter DLLs (via regsvr32)
  ClearErrors
  RegDLL "$OUTDIR\$vp8decoder_dll"
  RegDLL "$OUTDIR\$vp8encoder_dll"
  RegDLL "$OUTDIR\$webmmux_dll"
  RegDLL "$OUTDIR\$webmsource_dll"
  RegDLL "$OUTDIR\$webmsplit_dll"

  IfErrors RegError Success

  ; TODO(tomfinegan): needs error checking!

  ; skip over the error labels
  Goto Success

  RegError:
  ; some diagnostics
  DetailPrint "Filter DLL registration failed!"
  DetailPrint "OUTDIR: $OUTDIR"
  DetailPrint "vp8decoder_dll: $vp8decoder_dll"
  DetailPrint "vp8encoder_dll: $vp8encoder_dll"
  DetailPrint "webmmux_dll: $webmmux_dll"
  DetailPrint "webmsource_dll: $webmsource_dll"
  DetailPrint "webmsplit_dll: $webmsplit_dll"

  Success:

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall" SecUninstall

  ; Note: declarations are global, values are not...
  StrCpy $vp8decoder_dll "vp8decoder.dll"
  StrCpy $vp8encoder_dll "vp8encoder.dll"
  StrCpy $webmmux_dll "webmmux.dll"
  StrCpy $webmsource_dll "webmsource.dll"
  StrCpy $webmsplit_dll "webmsplit.dll"

  UnRegDLL "$INSTDIR\$vp8decoder_dll"
  UnRegDLL "$INSTDIR\$vp8encoder_dll"
  UnRegDLL "$INSTDIR\$webmmux_dll"
  UnRegDLL "$INSTDIR\$webmsource_dll"
  UnRegDLL "$INSTDIR\$webmsplit_dll"

  RMDir /r /REBOOTOK $INSTDIR

  DeleteRegKey HKCU "${WMDS_UNINSTALL_KEY}"

SectionEnd
