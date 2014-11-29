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

project(pulseview)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

option(DISABLE_WERROR "Build without -Werror" FALSE)
option(ENABLE_SIGNALS "Build with UNIX signals" TRUE)
option(ENABLE_DECODE "Build with libsigrokdecode" TRUE)
option(ENABLE_COTIRE "Enable cotire" FALSE)
option(ENABLE_TESTS "Enable unit tests" FALSE)
option(STATIC_PKGDEPS_LIBS "Statically link to (pkg-config) libraries" FALSE)
option(FORCE_QT4 "Force use of Qt4 even if Qt5 is available" FALSE)

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
	"Choose the type of build (None, Debug, Release, RelWithDebInfo, MinSizeRel)."
	FORCE)
endif()

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

list(APPEND PKGDEPS libsigrokxx>=0.3.0)

if(ENABLE_DECODE)
	list(APPEND PKGDEPS libsigrokdecode>=0.3.0)
endif()

if(ANDROID)
	list(APPEND PKGDEPS libsigrokandroidutils>=0.1.0)
endif()

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

if(FORCE_QT4)
	set(Qt5Core_FOUND FALSE)
else()
	find_package(Qt5Core QUIET)
endif()

if(Qt5Core_FOUND)
	message("-- Using Qt5")
	find_package(Qt5Widgets REQUIRED)
	find_package(Qt5Gui REQUIRED)
	find_package(Qt5Svg REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
	set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})
	set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::Svg)
	add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS})
else()
	find_program(QT_QMAKE_EXECUTABLE NAMES qmake4 qmake-qt4 qmake-mac)
	find_package(Qt4 REQUIRED QtCore QtGui QtSvg)
endif()

if(WIN32)
	# On Windows/MinGW we need to use 'thread_win32' instead of 'thread'.
	# The library is named libboost_thread_win32* (not libboost_thread*).
	find_package(Boost 1.42 COMPONENTS filesystem system thread_win32 REQUIRED)
else()
	find_package(Boost 1.42 COMPONENTS filesystem system thread REQUIRED)
endif()

# Find the platform's thread library (needed for C++11 threads).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads REQUIRED)

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
set(PV_VERSION_MINOR 2)
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
	pv/application.cpp
	pv/devicemanager.cpp
	pv/mainwindow.cpp
	pv/sigsession.cpp
	pv/storesession.cpp
	pv/util.cpp
	pv/data/analog.cpp
	pv/data/analogsnapshot.cpp
	pv/data/logic.cpp
	pv/data/logicsnapshot.cpp
	pv/data/signaldata.cpp
	pv/data/snapshot.cpp
	pv/dialogs/about.cpp
	pv/dialogs/connect.cpp
	pv/dialogs/storeprogress.cpp
	pv/popups/deviceoptions.cpp
	pv/popups/channels.cpp
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
	pv/view/cursorheader.cpp
	pv/view/cursorpair.cpp
	pv/view/header.cpp
	pv/view/marginwidget.cpp
	pv/view/logicsignal.cpp
	pv/view/rowitem.cpp
	pv/view/rowitemowner.cpp
	pv/view/ruler.cpp
	pv/view/selectableitem.cpp
	pv/view/signal.cpp
	pv/view/timemarker.cpp
	pv/view/trace.cpp
	pv/view/tracegroup.cpp
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
	pv/mainwindow.hpp
	pv/sigsession.hpp
	pv/storesession.hpp
	pv/dialogs/about.hpp
	pv/dialogs/connect.hpp
	pv/dialogs/storeprogress.hpp
	pv/popups/channels.hpp
	pv/popups/deviceoptions.hpp
	pv/prop/bool.hpp
	pv/prop/double.hpp
	pv/prop/enum.hpp
	pv/prop/int.hpp
	pv/prop/property.hpp
	pv/prop/string.hpp
	pv/prop/binding/deviceoptions.hpp
	pv/toolbars/samplingbar.hpp
	pv/view/cursor.hpp
	pv/view/cursorheader.hpp
	pv/view/header.hpp
	pv/view/logicsignal.hpp
	pv/view/marginwidget.hpp
	pv/view/rowitem.hpp
	pv/view/ruler.hpp
	pv/view/selectableitem.hpp
	pv/view/signal.hpp
	pv/view/timemarker.hpp
	pv/view/trace.hpp
	pv/view/tracegroup.hpp
	pv/view/view.hpp
	pv/view/viewport.hpp
	pv/widgets/colourbutton.hpp
	pv/widgets/colourpopup.hpp
	pv/widgets/popup.hpp
	pv/widgets/popuptoolbutton.hpp
	pv/widgets/sweeptimingwidget.hpp
	pv/widgets/wellarray.hpp
)

set(pulseview_FORMS
	pv/dialogs/about.ui
)

set(pulseview_RESOURCES
	pulseview.qrc
)

if(ENABLE_SIGNALS)
	list(APPEND pulseview_SOURCES signalhandler.cpp)
	list(APPEND pulseview_HEADERS signalhandler.hpp)
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
		pv/data/decoderstack.hpp
		pv/view/decodetrace.hpp
		pv/widgets/decodergroupbox.hpp
		pv/widgets/decodermenu.hpp
	)
endif()

if(WIN32)
	# Use the sigrok icon for the pulseview.exe executable.
	set(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -O coff -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")
	enable_language(RC)
	list(APPEND pulseview_SOURCES pulseviewico.rc)
endif()

if(ANDROID)
	list(APPEND pulseview_SOURCES android/loghandler.cpp)
endif()

if(Qt5Core_FOUND)
	qt5_wrap_cpp(pulseview_HEADERS_MOC ${pulseview_HEADERS})
	qt5_wrap_ui(pulseview_FORMS_HEADERS ${pulseview_FORMS})
	qt5_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})
else()
	qt4_wrap_cpp(pulseview_HEADERS_MOC ${pulseview_HEADERS})
	qt4_wrap_ui(pulseview_FORMS_HEADERS ${pulseview_FORMS})
	qt4_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})
	include(${QT_USE_FILE})
endif()

#===============================================================================
#= Global Definitions
#-------------------------------------------------------------------------------

add_definitions(${QT_DEFINITIONS} -DQT_NO_KEYWORDS)
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-Wall -Wextra)
add_definitions(-std=c++11)

if(ENABLE_DECODE)
	add_definitions(-DENABLE_DECODE)
endif()

if(NOT DISABLE_WERROR)
	add_definitions(-Werror)
endif()

if(ENABLE_SIGNALS)
	add_definitions(-DENABLE_SIGNALS)
endif()

#===============================================================================
#= Global Include Directories
#-------------------------------------------------------------------------------

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${Boost_INCLUDE_DIRS}
	${QT_INCLUDE_DIRS}
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
	${QT_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
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
	list(APPEND PULSEVIEW_LINK_LIBS "-lqsvg")
	list(APPEND PULSEVIEW_LINK_LIBS ${QT_QTSVG_LIBRARY})
endif()

if(ANDROID)
	list(APPEND PULSEVIEW_LINK_LIBS "-llog")
endif()

if(ANDROID)
add_library(${PROJECT_NAME} SHARED
	${pulseview_SOURCES}
	${pulseview_HEADERS_MOC}
	${pulseview_FORMS_HEADERS}
	${pulseview_RESOURCES_RCC}
)
else()
add_executable(${PROJECT_NAME}
	${pulseview_SOURCES}
	${pulseview_HEADERS_MOC}
	${pulseview_FORMS_HEADERS}
	${pulseview_RESOURCES_RCC}
)
endif()

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
