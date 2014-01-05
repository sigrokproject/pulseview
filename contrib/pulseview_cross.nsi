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
OutFile "pulseview-0.1.0-installer.exe"

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

# File name of the Python installer MSI file.
!define PY_INST "python-3.2.3.msi"

# Standard install path of the Python installer (do not change!).
!define PY_BIN "c:\Python32"

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

	# libusb0.dll (needed for libusb-0.1).
	File "${CROSS}/libusb0.dll"

	# Install the file(s) specified below into the specified directory.
	SetOutPath "$INSTDIR\decoders"

	# Protocol decoders.
	File /r /x "__pycache__" "${CROSS}/share/libsigrokdecode/decoders/*"

	# Install the file(s) specified below into the specified directory.
	SetOutPath "$INSTDIR\firmware"

	# Firmware files.
	File /r "${CROSS}/share/sigrok-firmware/*"

	# Generate the uninstaller executable.
	WriteUninstaller "$INSTDIR\Uninstall.exe"

	# Create a sub-directory in the start menu.
	CreateDirectory "$SMPROGRAMS\sigrok"
	CreateDirectory "$SMPROGRAMS\sigrok\PulseView"

	# Create a shortcut for the PulseView application.
	CreateShortCut "$SMPROGRAMS\sigrok\PulseView\PulseView.lnk" \
		"$INSTDIR\pulseview.exe" "" "$INSTDIR\pulseview.exe" \
		0 SW_SHOWNORMAL \
		"" "Open-source, portable sigrok GUI"

	# Create a shortcut for the uninstaller.
	CreateShortCut "$SMPROGRAMS\sigrok\PulseView\Uninstall.lnk" \
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
	File "${CROSS}/${PY_INST}"

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
	Delete "$INSTDIR\pulseview.exe"
	Delete "$INSTDIR\libusb0.dll"

	# Delete all decoders and everything else in decoders/.
	# There could be *.pyc files or __pycache__ subdirs and so on.
	RMDir /r "$INSTDIR\decoders\*"

	# Delete the firmware files.
	RMDir /r "$INSTDIR\firmware\*"

	# Delete the install directory and its sub-directories.
	RMDir "$INSTDIR\decoders"
	RMDir "$INSTDIR\firmware"
	RMDir "$INSTDIR"

	# Delete the links from the start menu.
	Delete "$SMPROGRAMS\sigrok\PulseView\PulseView.lnk"
	Delete "$SMPROGRAMS\sigrok\PulseView\Uninstall.lnk"

	# Delete the sub-directory in the start menu.
	RMDir "$SMPROGRAMS\sigrok\PulseView"
	RMDir "$SMPROGRAMS\sigrok"

	# Delete the registry key(s).
	DeleteRegKey HKLM "${REGSTR}"

SectionEnd


# --- Component selection section descriptions --------------------------------

LangString DESC_Section1 ${LANG_ENGLISH} "This installs the PulseView sigrok GUI, some firmware files, the protocol decoders, and all required libraries."
LangString DESC_Section2 ${LANG_ENGLISH} "This installs Python 3.2 in its default location of c:\Python32. If you already have Python 3.2 installed, you don't need to re-install it."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${Section1} $(DESC_Section1)
!insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DESC_Section2)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

