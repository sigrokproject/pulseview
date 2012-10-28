##
## This file is part of the PulseView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

cmake_minimum_required(VERSION 2.6)
include(FindPkgConfig)

project(pulseview)

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

option(ENABLE_TESTS "Enable unit tests" FALSE)
option(STATIC_PKGDEPS_LIBS "Statically link to (pkgconfig) libraries" FALSE)

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)
endif()

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED
	libsigrokdecode>=0.1.0
	libsigrok>=0.2.0
)

# On Windows/MinGW we explicitly point cmake to the Boost directory.
if(WIN32)
	set(BOOST_ROOT /usr/local)
endif()

find_package(Qt4 REQUIRED)

find_package(Boost 1.46)

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
	pv/about.cpp
	pv/datasnapshot.cpp
	pv/logicdata.cpp
	pv/logicdatasnapshot.cpp
	pv/logicsignal.cpp
	pv/mainwindow.cpp
	pv/samplingbar.cpp
	pv/signaldata.cpp
	pv/sigsession.cpp
	pv/signal.cpp
	pv/view/header.cpp
	pv/view/ruler.cpp
	pv/view/timemarker.cpp
	pv/view/view.cpp
	pv/view/viewport.cpp
)

set(pulseview_HEADERS
	pv/about.h
	pv/mainwindow.h
	pv/samplingbar.h
	pv/sigsession.h
	pv/view/header.h
	pv/view/ruler.h
	pv/view/view.h
	pv/view/viewport.h
)

set(pulseview_FORMS
	pv/about.ui
)

set(pulseview_RESOURCES
	pulseview.qrc
)

qt4_wrap_cpp(pulseview_HEADERS_MOC ${pulseview_HEADERS})
qt4_wrap_ui(pulseview_FORMS_HEADERS ${pulseview_FORMS})
qt4_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})

include(${QT_USE_FILE})

#===============================================================================
#= Global Definitions
#-------------------------------------------------------------------------------

add_definitions(${QT_DEFINITIONS})
add_definitions(-Werror -Wall)

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
	${QT_LIBRARIES}
)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND PULSEVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND PULSEVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

add_executable(${PROJECT_NAME}
	${pulseview_SOURCES}
	${pulseview_HEADERS_MOC}
	${pulseview_FORMS_HEADERS}
	${pulseview_RESOURCES_RCC}
)

target_link_libraries(${PROJECT_NAME} ${PULSEVIEW_LINK_LIBS})

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

install(PROGRAMS ${PROJECT_NAME} DESTINATION bin/)

#===============================================================================
#= Tests
#-------------------------------------------------------------------------------

if(ENABLE_TESTS)
	add_subdirectory(test)
	enable_testing()
	add_test(test ${CMAKE_CURRENT_BINARY_DIR}/test/pulseview-test)
endif(ENABLE_TESTS)
