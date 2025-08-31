;--------------------------------
; Use Modern UI 2
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "nsProcess.nsh"

;--------------------------------
; App details
Name "MuteActiveWindowC"
OutFile "MuteActiveWindowC-Setup.exe"
InstallDir "$PROGRAMFILES\MuteActiveWindowC"
InstallDirRegKey HKCU "Software\MuteActiveWindowC" ""
RequestExecutionLevel admin

;--------------------------------
; Start Menu page settings
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\MuteActiveWindowC"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

Var StartMenuFolder

;--------------------------------
; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES

; Finish page with "Run" checkbox and "Create Desktop Shortcut"
Function finishpageaction
  ; Create desktop shortcut
  CreateShortcut "$desktop\MuteActiveWindowC.lnk" "$instdir\MuteActiveWindowC.exe"
FunctionEnd

!define MUI_FINISHPAGE_RUN "$INSTDIR\MuteActiveWindowC.exe"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Desktop Shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION finishpageaction
!insertmacro MUI_PAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Kill process if running before install starts
Function .onInit
  ; Kill GUI if running
  nsProcess::_FindProcess "MuteActiveWindowC.exe"
  Pop $0
  ${If} $0 == 0
    nsProcess::_KillProcess "MuteActiveWindowC.exe"
    Pop $1
    Sleep 1000
  ${EndIf}
FunctionEnd

;--------------------------------
; Sections

; Main Application (always installed)
Section "Main Application" SecMain
  SectionIn RO ; Required section

  SetOutPath "$INSTDIR"
  File /r "MuteActiveWindowC\*.*"

  ; Save install path to registry
  WriteRegStr HKCU "Software\MuteActiveWindowC" "" $INSTDIR

  ; Create Start Menu shortcut
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortcut "$SMPROGRAMS\$StartMenuFolder\MuteActiveWindowC.lnk" "$INSTDIR\MuteActiveWindowC.exe"
  !insertmacro MUI_STARTMENU_WRITE_END

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

; Optional VC++ Redistributable Section (unchecked by default)
Section /o "Microsoft Visual C++ Redistributable" SecVCRedist
  DetailPrint "Downloading Microsoft Visual C++ Redistributable..."
  
  ; Download the VC++ redistributable to temp directory
  NSISdl::download "https://aka.ms/vs/17/release/vc_redist.x64.exe" "$TEMP\vc_redist.x64.exe"
  Pop $0 ; get the return value
  
  ; Check if download was successful
  StrCmp $0 success download_success
    MessageBox MB_ICONSTOP "Download failed: $0"
    Goto download_end
    
  download_success:
    DetailPrint "Installing Microsoft Visual C++ Redistributable..."
    
    ; Run the redistributable installer silently
    ExecWait '"$TEMP\vc_redist.x64.exe" /install /passive /norestart' $0
    
    ; Check installation result
    ${If} $0 == 0
      DetailPrint "Microsoft Visual C++ Redistributable installed successfully"
    ${ElseIf} $0 == 1638
      DetailPrint "Microsoft Visual C++ Redistributable is already installed (newer or same version)"
    ${Else}
      DetailPrint "Microsoft Visual C++ Redistributable installation failed (Exit code: $0)"
    ${EndIf}
    
    ; Clean up downloaded file
    Delete "$TEMP\vc_redist.x64.exe"
    
  download_end:
SectionEnd

;--------------------------------
; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "Main MuteActiveWindowC application (required)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVCRedist} "Download and install Microsoft Visual C++ Redistributable (recommended for compatibility)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; Uninstaller
Section "Uninstall"
  ; Kill GUI if running
  nsProcess::_FindProcess "MuteActiveWindowC.exe"
  Pop $0
  ${If} $0 == 0
    nsProcess::_KillProcess "MuteActiveWindowC.exe"
    Pop $1
    Sleep 1000
    nsProcess::_FindProcess "MuteActiveWindowC.exe"
    Pop $2
    ${If} $2 == 0
      MessageBox MB_OK|MB_ICONEXCLAMATION \
        "MuteActiveWindowC is still running and could not be closed. Please close it and retry."
      Quit
    ${EndIf}
  ${EndIf}

  ; Remove installed files
  RMDir /r "$INSTDIR"

  ; Remove Start Menu folder
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
  RMDir /r "$SMPROGRAMS\$StartMenuFolder"

  ; Remove desktop shortcut
  Delete "$DESKTOP\MuteActiveWindowC.lnk"

  ; Remove registry entries
  DeleteRegKey HKCU "Software\MuteActiveWindowC"
SectionEnd