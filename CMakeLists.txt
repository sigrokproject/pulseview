##
## This file is part of the PulseView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012-2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

cmake_minimum_required(VERSION 2.8.6)

include(FindPkgConfig)
include(GNUInstallDirs)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

project(pulseview)

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

option(DISABLE_WERROR "Build without -Werror" FALSE)
option(ENABLE_SIGNALS "Build with UNIX signals" TRUE)
option(ENABLE_DECODE "Build with libsigrokdecode" TRUE)
option(ENABLE_COTIRE "Enable cotire" FALSE)
option(ENABLE_TESTS "Enable unit tests" FALSE)
option(STATIC_PKGDEPS_LIBS "Statically link to (pkg-config) libraries" FALSE)

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# For boost-thread we need two additional settings on win32:
	set(Boost_USE_STATIC_LIBS ON)
	add_definitions(-DBOOST_THREAD_USE_LIB)

	# Windows does not support UNIX signals.
	set(ENABLE_SIGNALS FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

list(APPEND PKGDEPS libsigrok>=0.2.0)

if(ENABLE_DECODE)
	list(APPEND PKGDEPS libsigrokdecode>=0.2.0)
endif()

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

find_program(QT_QMAKE_EXECUTABLE NAMES qmake4 qmake-qt4 qmake-mac)
find_package(Qt4 REQUIRED)

# Find the platform's thread library (needed for boost-thread).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads)

if(WIN32)
	# On Windows/MinGW we need to use 'thread_win32' instead of 'thread'.
	# The library is named libboost_thread_win32* (not libboost_thread*).
	find_package(Boost 1.42 COMPONENTS filesystem system thread_win32 REQUIRED)
else()
	find_package(Boost 1.42 COMPONENTS filesystem system thread REQUIRED)
endif()

#===============================================================================
#= System Introspection
#-------------------------------------------------------------------------------

include(memaccess)
memaccess_check_unaligned_le(HAVE_UNALIGNED_LITTLE_ENDIAN_ACCESS)

#===============================================================================
#= Config Header
#-------------------------------------------------------------------------------

set(PV_TITLE PulseView)
set(PV_DESCRIPTION "A GUI for sigrok")

set(PV_VERSION_MAJOR 0)
set(PV_VERSION_MINOR 1)
set(PV_VERSION_MICRO 0)
set(PV_VERSION_STRING
	${PV_VERSION_MAJOR}.${PV_VERSION_MINOR}.${PV_VERSION_MICRO}
)

configure_file (
	${PROJECT_SOURCE_DIR}/config.h.in
	${PROJECT_BINARY_DIR}/config.h
)

#===============================================================================
#= Sources
#-------------------------------------------------------------------------------

set(pulseview_SOURCES
	main.cpp
	pv/devicemanager.cpp
	pv/mainwindow.cpp
	pv/sigsession.cpp
	pv/storesession.cpp
	pv/data/analog.cpp
	pv/data/analogsnapshot.cpp
	pv/data/logic.cpp
	pv/data/logicsnapshot.cpp
	pv/data/signaldata.cpp
	pv/data/snapshot.cpp
	pv/device/device.cpp
	pv/device/file.cpp
	pv/device/devinst.cpp
	pv/device/inputfile.cpp
	pv/device/sessionfile.cpp
	pv/dialogs/about.cpp
	pv/dialogs/connect.cpp
	pv/dialogs/storeprogress.cpp
	pv/popups/deviceoptions.cpp
	pv/popups/probes.cpp
	pv/prop/bool.cpp
	pv/prop/double.cpp
	pv/prop/enum.cpp
	pv/prop/int.cpp
	pv/prop/property.cpp
	pv/prop/string.cpp
	pv/prop/binding/binding.cpp
	pv/prop/binding/deviceoptions.cpp
	pv/toolbars/samplingbar.cpp
	pv/view/analogsignal.cpp
	pv/view/cursor.cpp
	pv/view/cursorpair.cpp
	pv/view/header.cpp
	pv/view/marginwidget.cpp
	pv/view/logicsignal.cpp
	pv/view/ruler.cpp
	pv/view/selectableitem.cpp
	pv/view/signal.cpp
	pv/view/timemarker.cpp
	pv/view/trace.cpp
	pv/view/tracepalette.cpp
	pv/view/view.cpp
	pv/view/viewport.cpp
	pv/widgets/colourbutton.cpp
	pv/widgets/colourpopup.cpp
	pv/widgets/popup.cpp
	pv/widgets/popuptoolbutton.cpp
	pv/widgets/sweeptimingwidget.cpp
	pv/widgets/wellarray.cpp
)

# This list includes only QObject derived class headers.
set(pulseview_HEADERS
	pv/mainwindow.h
	pv/sigsession.h
	pv/storesession.h
	pv/device/devinst.h
	pv/dialogs/about.h
	pv/dialogs/connect.h
	pv/dialogs/storeprogress.h
	pv/popups/probes.h
	pv/popups/deviceoptions.h
	pv/prop/bool.h
	pv/prop/double.h
	pv/prop/enum.h
	pv/prop/int.h
	pv/prop/property.h
	pv/prop/string.h
	pv/toolbars/samplingbar.h
	pv/view/cursor.h
	pv/view/header.h
	pv/view/logicsignal.h
	pv/view/marginwidget.h
	pv/view/ruler.h
	pv/view/selectableitem.h
	pv/view/signal.h
	pv/view/timemarker.h
	pv/view/trace.h
	pv/view/view.h
	pv/view/viewport.h
	pv/widgets/colourbutton.h
	pv/widgets/colourpopup.h
	pv/widgets/popup.h
	pv/widgets/popuptoolbutton.h
	pv/widgets/sweeptimingwidget.h
	pv/widgets/wellarray.h
)

set(pulseview_FORMS
	pv/dialogs/about.ui
)

set(pulseview_RESOURCES
	pulseview.qrc
)

if(ENABLE_SIGNALS)
	list(APPEND pulseview_SOURCES signalhandler.cpp)
	list(APPEND pulseview_HEADERS signalhandler.h)
endif()

if(ENABLE_DECODE)
	list(APPEND pulseview_SOURCES
		pv/data/decoderstack.cpp
		pv/data/decode/annotation.cpp
		pv/data/decode/decoder.cpp
		pv/data/decode/row.cpp
		pv/data/decode/rowdata.cpp
		pv/prop/binding/decoderoptions.cpp
		pv/view/decodetrace.cpp
		pv/widgets/decodergroupbox.cpp
		pv/widgets/decodermenu.cpp
	)

	list(APPEND pulseview_HEADERS
		pv/data/decoderstack.h
		pv/view/decodetrace.h
		pv/widgets/decodergroupbox.h
		pv/widgets/decodermenu.h
	)
endif()

if(WIN32)
	# Use the sigrok icon for the pulseview.exe executable.
	set(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -O coff -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")
	enable_language(RC)
	list(APPEND pulseview_SOURCES pulseviewico.rc)
endif()

qt4_wrap_cpp(pulseview_HEADERS_MOC ${pulseview_HEADERS})
qt4_wrap_ui(pulseview_FORMS_HEADERS ${pulseview_FORMS})
qt4_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})

include(${QT_USE_FILE})

#===============================================================================
#= Global Definitions
#-------------------------------------------------------------------------------

add_definitions(${QT_DEFINITIONS})
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-Wall -Wextra)

if(ENABLE_DECODE)
	add_definitions(-DENABLE_DECODE)
endif()

if(NOT DISABLE_WERROR)
	add_definitions(-Werror)
endif()

#===============================================================================
#= Global Include Directories
#-------------------------------------------------------------------------------

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${Boost_INCLUDE_DIRS}
)

if(STATIC_PKGDEPS_LIBS)
	include_directories(${PKGDEPS_STATIC_INCLUDE_DIRS})
else()
	include_directories(${PKGDEPS_INCLUDE_DIRS})
endif()

#===============================================================================
#= Linker Configuration
#-------------------------------------------------------------------------------

link_directories(${Boost_LIBRARY_DIRS})

set(PULSEVIEW_LINK_LIBS
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND PULSEVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND PULSEVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

if(WIN32)
	# On Windows we need to statically link the libqsvg imageformat
	# plugin (and the QtSvg component) for SVG graphics/icons to work.
	add_definitions(-DQT_STATICPLUGIN)
	link_directories("${QT_PLUGINS_DIR}/imageformats")
	list(APPEND PULSEVIEW_LINK_LIBS ${QT_QTSVG_LIBRARY})
	list(APPEND PULSEVIEW_LINK_LIBS "-lqsvg")
endif()

add_executable(${PROJECT_NAME}
	${pulseview_SOURCES}
	${pulseview_HEADERS_MOC}
	${pulseview_FORMS_HEADERS}
	${pulseview_RESOURCES_RCC}
)

target_link_libraries(${PROJECT_NAME} ${PULSEVIEW_LINK_LIBS})

if(WIN32)
	# Pass -mwindows so that no "DOS box" opens when PulseView is started.
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()

if(ENABLE_COTIRE)
	include(cotire)
	cotire(${PROJECT_NAME})
endif()

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
install(TARGETS ${PROJECT_NAME} DESTINATION bin/)

# Install the manpage.
install(FILES doc/pulseview.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT doc)

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${PV_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PV_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PV_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${PV_VERSION_MAJOR}.${PV_VERSION_MINOR}.${PV_VERSION_MICRO}")
set(CPACK_SOURCE_GENERATOR "TGZ")

include(CPack)

#===============================================================================
#= Tests
#-------------------------------------------------------------------------------

if(ENABLE_TESTS)
	add_subdirectory(test)
	enable_testing()
	add_test(test ${CMAKE_CURRENT_BINARY_DIR}/test/pulseview-test)
endif()
