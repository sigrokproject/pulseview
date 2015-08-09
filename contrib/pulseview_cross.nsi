##
## This file is part of the PulseView project.
##
## Copyright (C) 2013-2014 Uwe Hermann <uwe@hermann-uwe.de>
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
# It is meant for use in a cross-compile setup (not for native builds).
# See the 'sigrok-cross-mingw' script in the sigrok-util repo for details.
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
OutFile "pulseview-0.2.0-installer.exe"

# Where to install the application.
InstallDir "$PROGRAMFILES\sigrok\PulseView"

# Request admin privileges for Windows Vista and Windows 7.
# http://nsis.sourceforge.net/Docs/Chapter4.html
RequestExecutionLevel admin

# Local helper definitions.
!define REGSTR "Software\Microsoft\Windows\CurrentVersion\Uninstall\PulseView"


# --- MUI interface configuration ---------------------------------------------

# Use the following icon for the installer EXE file.
!define MUI_ICON "../icons/sigrok-logo-notext.ico"

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

# Path where the cross-compiled sigrok tools and libraries are located.
# Change this to where-ever you installed libsigrok.a and so on.
!define CROSS "$%HOME%/sr_mingw"

# Path where the cross-compiled MXE tools and libraries are located.
# Change this to where-ever you installed MXE (and the files it built).
!define MXE "$%HOME%/mxe-git/usr/i686-pc-mingw32"


# --- MUI pages ---------------------------------------------------------------

# Show a nice "Welcome to the ... Setup Wizard" page.
!insertmacro MUI_PAGE_WELCOME

# Show the license of the project.
!insertmacro MUI_PAGE_LICENSE "../COPYING"

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

	# License file.
	File "../COPYING"

	# PulseView (statically linked, includes all libs).
	File "${CROSS}/bin/pulseview.exe"

	# Zadig (used for installing WinUSB drivers).
	File "${CROSS}/zadig.exe"
	File "${CROSS}/zadig_xp.exe"

	# Python
	File "${CROSS}/python32.dll"
	File "${CROSS}/python32.zip"

	# Protocol decoders.
	SetOutPath "$INSTDIR\decoders"
	File /r /x "__pycache__" "${CROSS}/share/libsigrokdecode/decoders/*"

	# Firmware files.
	SetOutPath "$INSTDIR\firmware"
	File /r "${CROSS}/share/sigrok-firmware/*"

	# Example *.sr files.
	SetOutPath "$INSTDIR\examples"
	File /r "${CROSS}/share/sigrok-dumps/*"

	# Generate the uninstaller executable.
	WriteUninstaller "$INSTDIR\Uninstall.exe"

	# Create a sub-directory in the start menu.
	CreateDirectory "$SMPROGRAMS\sigrok"
	CreateDirectory "$SMPROGRAMS\sigrok\PulseView"

	# Create a shortcut for the PulseView application.
	SetOutPath "$INSTDIR"
	CreateShortCut "$SMPROGRAMS\sigrok\PulseView\PulseView.lnk" \
		"$INSTDIR\pulseview.exe" "" "$INSTDIR\pulseview.exe" \
		0 SW_SHOWNORMAL \
		"" "Open-source, portable sigrok GUI"

	# Create a shortcut for the uninstaller.
	CreateShortCut "$SMPROGRAMS\sigrok\PulseView\Uninstall.lnk" \
		"$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0 \
		SW_SHOWNORMAL "" "Uninstall PulseView"

	# Create a shortcut for the Zadig executable.
	CreateShortCut "$SMPROGRAMS\sigrok\PulseView\Zadig.lnk" \
		"$INSTDIR\zadig.exe" "" "$INSTDIR\zadig.exe" 0 \
		SW_SHOWNORMAL "" "Zadig"

	# Create a shortcut for the Zadig executable (for Win XP).
	CreateShortCut "$SMPROGRAMS\sigrok\PulseView\Zadig (Win XP).lnk" \
		"$INSTDIR\zadig_xp.exe" "" "$INSTDIR\zadig_xp.exe" 0 \
		SW_SHOWNORMAL "" "Zadig (Win XP)"

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
	WriteRegStr HKLM "${REGSTR}" "DisplayVersion" "0.2.0"
	WriteRegStr HKLM "${REGSTR}" "Contact" \
		"sigrok-devel@lists.sourceforge.org"
	WriteRegStr HKLM "${REGSTR}" "Comments" \
		"This is a Qt based sigrok GUI."

	# Display "Remove" instead of "Modify/Remove" in the control panel.
	WriteRegDWORD HKLM "${REGSTR}" "NoModify" 1
	WriteRegDWORD HKLM "${REGSTR}" "NoRepair" 1

SectionEnd


# --- Uninstaller section -----------------------------------------------------

Section "Uninstall"

	# Always delete the uninstaller first (yes, this really works).
	Delete "$INSTDIR\Uninstall.exe"

	# Delete the application, the application data, and related libs.
	Delete "$INSTDIR\COPYING"
	Delete "$INSTDIR\pulseview.exe"
	Delete "$INSTDIR\zadig.exe"
	Delete "$INSTDIR\zadig_xp.exe"
	Delete "$INSTDIR\python32.dll"
	Delete "$INSTDIR\python32.zip"

	# Delete all decoders and everything else in decoders/.
	# There could be *.pyc files or __pycache__ subdirs and so on.
	RMDir /r "$INSTDIR\decoders\*"

	# Delete the firmware files.
	RMDir /r "$INSTDIR\firmware\*"

	# Delete the example *.sr files.
	RMDir /r "$INSTDIR\examples\*"

	# Delete the install directory and its sub-directories.
	RMDir "$INSTDIR\decoders"
	RMDir "$INSTDIR\firmware"
	RMDir "$INSTDIR\examples"
	RMDir "$INSTDIR"

	# Delete the links from the start menu.
	Delete "$SMPROGRAMS\sigrok\PulseView\PulseView.lnk"
	Delete "$SMPROGRAMS\sigrok\PulseView\Uninstall.lnk"
	Delete "$SMPROGRAMS\sigrok\PulseView\Zadig.lnk"
	Delete "$SMPROGRAMS\sigrok\PulseView\Zadig (Win XP).lnk"

	# Delete the sub-directory in the start menu.
	RMDir "$SMPROGRAMS\sigrok\PulseView"
	RMDir "$SMPROGRAMS\sigrok"

	# Delete the registry key(s).
	DeleteRegKey HKLM "${REGSTR}"

SectionEnd


# --- Component selection section descriptions --------------------------------

LangString DESC_Section1 ${LANG_ENGLISH} "This installs the PulseView sigrok GUI, some firmware files, the protocol decoders, some example files, and all required libraries."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${Section1} $(DESC_Section1)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

