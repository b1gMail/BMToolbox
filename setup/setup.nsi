; $Id: $

;--------------------------------
;Functions

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;General

  ;Name and file
  !define SHORT_APP_NAME "BMToolbox-%%serviceID%%"
  Name "%%appName%%"
  OutFile "setup.exe"
  BrandingText "%%brandingText%%" ; powered by b1gMail
  RequestExecutionLevel user
  AutoCloseWindow true

  ;Default installation folder
  InstallDir "$LOCALAPPDATA\BMToolbox-%%serviceID%%"

  ;Get installation folder from registry if available
  ;InstallDirRegKey HKCU "Software\BMToolbox" ""

;--------------------------------
;Interface Settings

;  !define MUI_ICON ".\install.ico"
  !define MUI_ICON ".\src\res\app-ico.ico"
  !define MUI_UNICON ".\uninstall.ico"
  !define MUI_ABORTWARNING
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_RIGHT
  !define MUI_HEADERIMAGE_BITMAP ".\src\res\wizard-head.bmp"
  !define MUI_WELCOMEFINISHPAGE_BITMAP ".\src\res\wizard-left.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP ".\src\res\wizard-left.bmp"
;  !define MUI_FINISHPAGE_RUN_TEXT "BMToolbox starten"
;  !define MUI_FINISHPAGE_RUN "$INSTDIR\BMToolbox.exe"

;--------------------------------
;Pages

  !define MUI_WELCOMEPAGE_TITLE_3LINES
  !insertmacro MUI_PAGE_WELCOME
;  !insertmacro MUI_PAGE_LICENSE "license.rtf"
;  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
;  !insertmacro MUI_PAGE_FINISH

  !define MUI_WELCOMEPAGE_TITLE_3LINES
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !define MUI_WELCOMEPAGE_TITLE_3LINES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Slovenian"

;--------------------------------
;Installer Sections

Section "BMToolbox" SecBMToolbox
  ; todo: close if running
  IfFileExists $INSTDIR\BMToolbox.exe Exists DoesNotExist
Exists:
  ExecWait '"$INSTDIR\BMToolbox.exe" -close'
DoesNotExist:

  ; app
  SetOutPath "$INSTDIR"
  SetShellVarContext current
  File "src\BMToolbox.exe"
  File "src\bmtoolbox_en.qm"
  File "src\bmtoolbox_de.qm"
  File "src\qt_de.qm"
  File "src\branding.ini"
  File "src\license.html"		; rtf?
  File "src\QtCore4.dll"
  File "src\QtGui4.dll"
  File "src\QtNetwork4.dll"
  File "src\QtSql4.dll"
  File "src\QtXml4.dll"
  File "src\QtSingleApplication.dll"
  File "src\libeay32.dll"
  File "src\ssleay32.dll"
  File "src\msvcr100.dll"
  File "src\msvcp100.dll"

  ; fax dir
  CreateDirectory "$INSTDIR\fax"

  ; bin
  SetOutPath "$INSTDIR\faxdrv"
  File "src\faxdrv\bmfaxmon32.dll"
  File "src\faxdrv\bmfaxmon64.dll"
  File "src\faxdrv\bmfaxprint.gpd"
  File "src\faxdrv\bmfaxprint.inf"
  File "src\faxdrv\BMFaxPrintInstall.exe"

  ; sqldrivers
  SetOutPath "$INSTDIR\sqldrivers"
  File "src\sqldrivers\qsqlite4.dll"

  ; imageformats
  SetOutPath "$INSTDIR\imageformats"
  File "src\imageformats\qgif4.dll"

  ; res
  SetOutPath "$INSTDIR\res"
  File "src\res\app-ico.png"
  File "src\res\app-ico.ico"
  File "src\res\wizard-head.bmp"
;  File "src\res\wizard-left.bmp"
  File "src\res\newmail.wav"

  ; uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; shortcuts
  CreateDirectory "$SMPROGRAMS\%%appName%%"
  CreateShortCut "$SMPROGRAMS\%%appName%%\%%appName%%.lnk" $INSTDIR\BMToolbox.exe "" $INSTDIR\res\app-ico.ico
  CreateShortCut "$SMPROGRAMS\%%appName%%\%%appName%% %%nameSMSManager%%.lnk" $INSTDIR\BMToolbox.exe "-smsmanager" $INSTDIR\res\app-ico.ico
  CreateShortCut "$SMPROGRAMS\%%appName%%\%%appName%% deinstallieren.lnk" $INSTDIR\Uninstall.exe

  ; uninstaller registry
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BMToolbox-%%serviceID%%" \
                 "DisplayName" "%%appName%%"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BMToolbox-%%serviceID%%" \
                 "UninstallString" "$INSTDIR\Uninstall.exe"

  ; exec
  Exec "$INSTDIR\BMToolbox.exe"
SectionEnd


;--------------------------------
;Uninstaller Section

Section "Uninstall"
  SetShellVarContext current

  ; todo: uninstall fax printer
  ; todo: remove registry settings
  ; todo: remove autostart entry
  ; todo: exit app

  ; exit app
  ExecWait '"$INSTDIR\BMToolbox.exe" -uninstall'

  ; delete shortcuts
  Delete "$SMPROGRAMS\%%appName%%\%%appName%%.lnk"
  Delete "$SMPROGRAMS\%%appName%%\%%appName%% %%nameSMSManager%%.lnk"
  Delete "$SMPROGRAMS\%%appName%%\%%appName%% deinstallieren.lnk"
  RMDir "$SMPROGRAMS\%%appName%%"

  ; delete files
  Delete "$INSTDIR\Uninstall.exe"

  ; delete folder
  RMDir /R /REBOOTOK "$INSTDIR"

  ; delete registry keys
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BMToolbox-%%serviceID%%"
SectionEnd
