; VOLTA Resonance — Windows NSIS Installer Script
; Build with: makensis /DBUILD_DIR=<path> /DDIST_DIR=<path> volta-resonance.nsi
; Requires NSIS 3.x — https://nsis.sourceforge.io/Download

!include "MUI2.nsh"

; ── Metadata ──────────────────────────────────────────────────────────────────
Name              "VOLTA Resonance"
OutFile           "${DIST_DIR}\VOLTA Resonance Installer.exe"
Unicode           True
RequestExecutionLevel admin

; VST3 standard install location (64-bit Program Files\Common Files\VST3)
InstallDir        "$COMMONFILES64\VST3"

; ── MUI Interface ─────────────────────────────────────────────────────────────
!define MUI_WELCOMEPAGE_TITLE "VOLTA Resonance v1.1"
!define MUI_WELCOMEPAGE_TEXT  "This wizard will install the VOLTA Resonance VST3 \
drum synthesiser plugin.$\r$\n$\r$\nVOLTA Resonance is a collaborative 12-voice drum \
machine with built-in step sequencer and real-time session sync.$\r$\n$\r$\nClick Next \
to continue."

!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_TEXT   "VOLTA Resonance has been installed successfully.\
$\r$\n$\r$\nTo use the plugin in Ableton Live:$\r$\n\
  1. Open Preferences > Plug-Ins$\r$\n\
  2. Enable VST3 Plug-In Custom Folder or check VST3 system folder$\r$\n\
  3. Click Rescan Plug-Ins$\r$\n\
  4. Add a MIDI track and load VOLTA Resonance from the instrument browser"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install section ───────────────────────────────────────────────────────────
Section "VOLTA Resonance VST3" SecMain
    SectionIn RO   ; required — cannot be deselected

    ; Use 64-bit registry and filesystem paths
    SetRegView 64

    ; Copy VST3 bundle to C:\Program Files\Common Files\VST3\
    SetOutPath "$COMMONFILES64\VST3"
    File /r "${BUILD_DIR}\VoltaResonance_artefacts\Release\VST3\VOLTA Resonance.vst3"

    ; Write uninstaller to its own directory
    SetOutPath "$PROGRAMFILES64\Volta Labs\VOLTA Resonance"
    WriteUninstaller "$PROGRAMFILES64\Volta Labs\VOLTA Resonance\uninstall.exe"

    ; Add/Remove Programs registration
    WriteRegStr  HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "DisplayName"      "VOLTA Resonance"
    WriteRegStr  HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "DisplayVersion"   "1.1"
    WriteRegStr  HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "Publisher"        "Volta Labs"
    WriteRegStr  HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "InstallLocation"  "$COMMONFILES64\VST3"
    WriteRegStr  HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "UninstallString"  "$PROGRAMFILES64\Volta Labs\VOLTA Resonance\uninstall.exe"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance" \
                 "NoRepair" 1
SectionEnd

; ── Uninstall section ─────────────────────────────────────────────────────────
Section "Uninstall"
    SetRegView 64

    RMDir /r "$COMMONFILES64\VST3\VOLTA Resonance.vst3"
    Delete   "$PROGRAMFILES64\Volta Labs\VOLTA Resonance\uninstall.exe"
    RMDir    "$PROGRAMFILES64\Volta Labs\VOLTA Resonance"
    RMDir    "$PROGRAMFILES64\Volta Labs"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoltaResonance"
SectionEnd
