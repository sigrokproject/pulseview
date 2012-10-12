##
## This file is part of the PulseView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED
	libsigrokdecode>=0.1.0
	libsigrok>=0.2.0
)

# On Windows/MinGW we explicitly point cmake to the Boost directory.
if(WIN32)
set(BOOST_ROOT /usr/local)
endif(WIN32)

find_package(Qt4 REQUIRED)
find_package(Boost 1.46 COMPONENTS unit_test_framework REQUIRED)

set(VERSION 0.1.0)

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

set(pulseview_TEST_SOURCES
	pv/datasnapshot.cpp
	pv/logicdatasnapshot.cpp
	test/logicdatasnapshot.cpp
	test/test.cpp
)

qt4_wrap_cpp(pulseview_HEADERS_MOC ${pulseview_HEADERS})
qt4_wrap_ui(pulseview_FORMS_HEADERS ${pulseview_FORMS})
qt4_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})

include(${QT_USE_FILE})

add_definitions(${QT_DEFINITIONS})
add_definitions(-DAPP_VERSION="${VERSION}")

# On Windows/MinGW we need PKGDEPS_STATIC_INCLUDE_DIRS.
if(WIN32)
include_directories(
	${include_directories}
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIRS}
	${PKGDEPS_STATIC_INCLUDE_DIRS}
)
else(WIN32)
include_directories(
	${include_directories}
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIRS}
	${PKGDEPS_INCLUDE_DIRS}
)
endif(WIN32)

# On Windows/MinGW we need PKGDEPS_STATIC_LIBRARY_DIRS.
if(WIN32)
link_directories(
	${Boost_LIBRARY_DIRS}
	${PKGDEPS_STATIC_LIBRARY_DIRS}
)
else(WIN32)
link_directories(
	${Boost_LIBRARY_DIRS}
	${PKGDEPS_LIBRARY_DIRS}
)
endif(WIN32)

add_executable(pulseview
	${pulseview_SOURCES}
	${pulseview_HEADERS_MOC}
	${pulseview_FORMS_HEADERS}
	${pulseview_RESOURCES_RCC}
)

# On Windows/MinGW we need PKGDEPS_STATIC_LIBRARIES.
if(WIN32)
target_link_libraries(pulseview
	${Boost_LIBRARIES}
	${PKGDEPS_STATIC_LIBRARIES}
	${QT_LIBRARIES}
)
else(WIN32)
target_link_libraries(pulseview
	${Boost_LIBRARIES}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
)
endif(WIN32)

install(PROGRAMS pulseview DESTINATION bin/)

if(ENABLE_TESTS)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(pulseview-test
	${pulseview_TEST_SOURCES}
)

# On Windows/MinGW we need PKGDEPS_STATIC_LIBRARIES.
if(WIN32)
target_link_libraries(pulseview-test
	${Boost_LIBRARIES}
	${PKGDEPS_STATIC_LIBRARIES}
	${QT_LIBRARIES}
)
else(WIN32)
target_link_libraries(pulseview-test
	${Boost_LIBRARIES}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
)
endif(WIN32)

enable_testing()
add_test(test ${CMAKE_CURRENT_BINARY_DIR}/pulseview-test)

endif(ENABLE_TESTS)
