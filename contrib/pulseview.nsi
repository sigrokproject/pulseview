##
## This file is part of the PulseView project.
##
## Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

#
# This file is used to create the PulseView Windows installer via NSIS.
#
# NSIS documentation:
# http://nsis.sourceforge.net/Docs/
# http://nsis.sourceforge.net/Docs/Modern%20UI%202/Readme.html
#

# Include the "Modern UI" header, which gives us the usual Windows look-n-feel.
!include "MUI2.nsh"


# --- Global stuff ------------------------------------------------------------

# Installer/product name.
Name "PulseView"

# Filename of the installer executable.
OutFile "pulseview-0.1.0-installer.exe"

# Where to install the application.
InstallDir "$PROGRAMFILES\sigrok\pulseview"

# Request admin privileges for Windows Vista and Windows 7.
# http://nsis.sourceforge.net/Docs/Chapter4.html
RequestExecutionLevel admin

# Local helper definitions.
!define REGSTR "Software\Microsoft\Windows\CurrentVersion\Uninstall\pulseview"


# --- MUI interface configuration ---------------------------------------------

# Use the following icon for the installer EXE file.
!define MUI_ICON "..\icons\sigrok-logo-notext.ico"

# Show a nice image at the top of each installer page.
!define MUI_HEADERIMAGE

# Don't automatically go to the Finish page so the user can check the log.
!define MUI_FINISHPAGE_NOAUTOCLOSE

# Upon "cancel", ask the user if he really wants to abort the installer.
!define MUI_ABORTWARNING

# Don't force the user to accept the license, just show it.
# Details: http://trac.videolan.org/vlc/ticket/3124
!define MUI_LICENSEPAGE_BUTTON $(^NextBtn)
!define MUI_LICENSEPAGE_TEXT_BOTTOM "Click Next to continue."

# File name of the Python installer MSI file.
!define PY_INST "python-3.2.2.msi"

# Standard install path of the Python installer (do not change!).
!define PY_BIN "c:\Python32"


# --- MUI pages ---------------------------------------------------------------

# Show a nice "Welcome to the ... Setup Wizard" page.
!insertmacro MUI_PAGE_WELCOME

# Show the license of the project.
!insertmacro MUI_PAGE_LICENSE "..\COPYING"

# Show a screen which allows the user to select which components to install.
!insertmacro MUI_PAGE_COMPONENTS

# Allow the user to select a different install directory.
!insertmacro MUI_PAGE_DIRECTORY

# Perform the actual installation, i.e. install the files.
!insertmacro MUI_PAGE_INSTFILES

# Show a final "We're done, click Finish to close this wizard" message.
!insertmacro MUI_PAGE_FINISH

# Pages used for the uninstaller.
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH


# --- MUI language files ------------------------------------------------------

# Select an installer language (required!).
!insertmacro MUI_LANGUAGE "English"


# --- Default section ---------------------------------------------------------

Section "PulseView (required)" Section1

	# This section is gray (can't be disabled) in the component list.
	SectionIn RO

	# Install the file(s) specified below into the specified directory.
	SetOutPath "$INSTDIR"

	# License file
	File "..\COPYING"

	# sigrok libs
	File "c:\MinGW\msys\1.0\local\lib\libsigrok.a"
	File "c:\MinGW\msys\1.0\local\lib\libsigrokdecode.a"

	# PulseView
	File "c:\MinGW\msys\1.0\local\bin\pulseview.exe"

	# Icon
	File "..\icons\sigrok-logo-notext.ico"

	# MinGW libs
	File "c:\MinGW\bin\mingwm10.dll"
	File "c:\MinGW\bin\libgcc_s_dw2-1.dll"
	File "c:\MinGW\bin\intl.dll"
	File "c:\MinGW\bin\libiconv-2.dll"
	File "c:\MinGW\bin\libstdc++-6.dll"

	# MSYS libs.
	File "c:\MinGW\msys\1.0\bin\msys-1.0.dll"
	File "c:\MinGW\msys\1.0\bin\msys-z.dll"

	# External libs
	File "c:\MinGW\msys\1.0\local\bin\libglib-2.0-0.dll"
	File "c:\MinGW\msys\1.0\local\bin\libgthread-2.0-0.dll"
	File "c:\MinGW\msys\1.0\local\bin\libusb-1.0.dll"
	File "c:\MinGW\msys\1.0\local\bin\zlib1.dll"
	File "c:\MinGW\msys\1.0\local\bin\libzip-2.dll"
	File "c:\MinGW\msys\1.0\local\bin\libusb0.dll"
	File "c:\MinGW\msys\1.0\local\bin\libftdi.dll"

	# Qt libs
	File "c:\QtSDK\Desktop\Qt\4.7.4\mingw\bin\QtCore4.dll"
	File "c:\QtSDK\Desktop\Qt\4.7.4\mingw\bin\QtGui4.dll"

	# Install the file(s) specified below into the specified directory.
	SetOutPath "$INSTDIR\decoders"

	# Protocol decoders
	File /r /x "__pycache__" \
		"c:\MinGW\msys\1.0\local\share\libsigrokdecode\decoders\*"

	# Generate the uninstaller executable.
	WriteUninstaller "$INSTDIR\Uninstall.exe"

	# Create a sub-directory in the start menu.
	CreateDirectory "$SMPROGRAMS\sigrok"
	CreateDirectory "$SMPROGRAMS\sigrok\pulseview"

	# Create a shortcut for PulseView application.
	CreateShortCut "$SMPROGRAMS\sigrok\pulseview\PulseView.lnk" \
		"$INSTDIR\pulseview.exe" "" "$INSTDIR\pulseview.exe" \
		0 SW_SHOWNORMAL \
		"" "Open-source, portable logic analyzer software"

	# Create a shortcut for the uninstaller.
	CreateShortCut "$SMPROGRAMS\sigrok\pulseview\Uninstall.lnk" \
		"$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0 \
		SW_SHOWNORMAL "" "Uninstall PulseView"

	# Create registry keys for "Add/remove programs" in the control panel.
	WriteRegStr HKLM "${REGSTR}" "DisplayName" "PulseView"
	WriteRegStr HKLM "${REGSTR}" "UninstallString" \
		"$\"$INSTDIR\Uninstall.exe$\""
	WriteRegStr HKLM "${REGSTR}" "InstallLocation" "$\"$INSTDIR$\""
	WriteRegStr HKLM "${REGSTR}" "DisplayIcon" \
		"$\"$INSTDIR\sigrok-logo-notext.ico$\""
	WriteRegStr HKLM "${REGSTR}" "Publisher" "sigrok"
	WriteRegStr HKLM "${REGSTR}" "HelpLink" \
		"http://sigrok.org/wiki/PulseView"
	WriteRegStr HKLM "${REGSTR}" "URLUpdateInfo" \
		"http://sigrok.org/wiki/Downloads"
	WriteRegStr HKLM "${REGSTR}" "URLInfoAbout" "http://sigrok.org"
	WriteRegStr HKLM "${REGSTR}" "DisplayVersion" "0.1.0"
	WriteRegStr HKLM "${REGSTR}" "Contact" \
		"sigrok-devel@lists.sourceforge.org"
	WriteRegStr HKLM "${REGSTR}" "Comments" \
		"This is a Qt based sigrok GUI."

	# Display "Remove" instead of "Modify/Remove" in the control panel.
	WriteRegDWORD HKLM "${REGSTR}" "NoModify" 1
	WriteRegDWORD HKLM "${REGSTR}" "NoRepair" 1

SectionEnd


# --- Python installer section ------------------------------------------------

Section "Python" Section2

	# Copy the Python installer MSI file into the temporary directory.
	SetOutPath "$TEMP"
	File "c:\${PY_INST}"

	# Run the Python installer MSI file from within our installer.
	ExecWait '"msiexec" /i "$TEMP\${PY_INST}" /QB- /passive ALLUSERS=1'

	# Remove Python installer MSI file again.
	Delete "$TEMP\${PY_INST}"

SectionEnd


# --- Uninstaller section -----------------------------------------------------

Section "Uninstall"

	# Always delete the uninstaller first (yes, this really works).
	Delete "$INSTDIR\Uninstall.exe"

	# Delete the application, the application data, and related libs.
	Delete "$INSTDIR\COPYING"
	Delete "$INSTDIR\libsigrok.a"
	Delete "$INSTDIR\libsigrokdecode.a"
	Delete "$INSTDIR\pulseview.exe"
	Delete "$INSTDIR\sigrok-logo-notext.ico"
	Delete "$INSTDIR\mingwm10.dll"
	Delete "$INSTDIR\libgcc_s_dw2-1.dll"
	Delete "$INSTDIR\intl.dll"
	Delete "$INSTDIR\libiconv-2.dll"
	Delete "$INSTDIR\libstdc++-6.dll"
	Delete "$INSTDIR\msys-1.0.dll"
	Delete "$INSTDIR\msys-z.dll"
	Delete "$INSTDIR\libglib-2.0-0.dll"
	Delete "$INSTDIR\libgthread-2.0-0.dll"
	Delete "$INSTDIR\libusb-1.0.dll"
	Delete "$INSTDIR\zlib1.dll"
	Delete "$INSTDIR\libzip-2.dll"
	Delete "$INSTDIR\libusb0.dll"
	Delete "$INSTDIR\libftdi.dll"
	Delete "$INSTDIR\QtCore4.dll"
	Delete "$INSTDIR\QtGui4.dll"

	# Delete all decoders and everything else in decoders/.
	# There could be *.pyc files or __pycache__ subdirs and so on.
	RMDir /r "$INSTDIR\decoders\*"

	# Delete the install directory and its sub-directories.
	RMDir "$INSTDIR\decoders"
	RMDir "$INSTDIR"

	# Delete the links from the start menu.
	Delete "$SMPROGRAMS\sigrok\pulseview\PulseView.lnk"
	Delete "$SMPROGRAMS\sigrok\pulseview\Uninstall.lnk"

	# Delete the sub-directory in the start menu.
	RMDir "$SMPROGRAMS\sigrok\pulseview"
	RMDir "$SMPROGRAMS\sigrok"

	# Delete the registry key(s).
	DeleteRegKey HKLM "${REGSTR}"

SectionEnd


# --- Component selection section descriptions --------------------------------

LangString DESC_Section1 ${LANG_ENGLISH} "This installs the PulseView GUI, the protocol decoders, and all required libraries."
LangString DESC_Section2 ${LANG_ENGLISH} "This installs Python 3.2 in its default location of c:\Python32. If you already have Python 3.2 installed, you don't need to re-install it."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${Section1} $(DESC_Section1)
!insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DESC_Section2)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

