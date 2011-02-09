;
; Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
; Use of this source code is governed by a BSD-style license and patent
; grant that can be found in the LICENSE file in the root of the source
; tree. All contributing project authors may be found in the AUTHORS
; file in the root of the source tree.
;
!ifndef __MEDIAFOUNDATION_INSTALLER_WEBMMF_UTIL_NSH__
!define  __MEDIAFOUNDATION_INSTALLER_WEBMMF_UTIL_NSH__

!macro RegDLL64 com_dll
  ExecWait '"$SYSDIR\regsvr32.exe" /s "${com_dll}"'
  IfErrors 0 +2
    DetailPrint "${com_dll} failed to register."
  ClearErrors
!macroend

!macro UnRegDLL64 com_dll
  ExecWait '"$SYSDIR\regsvr32.exe" /s /u "${com_dll}"'
  IfErrors 0 +2
    DetailPrint "${com_dll} failed to unregister."
  ClearErrors
!macroend

!endif ; __MEDIAFOUNDATION_INSTALLER_WEBMMF_UTIL_NSH__

