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

option(ENABLE_DECODE "Build with libsigrokdecode" FALSE)

list(APPEND PKGDEPS libsigrok>=0.3.0)

if(ENABLE_DECODE)
	list(APPEND PKGDEPS libsigrokdecode>=0.3.0)
endif()

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

if(WIN32)
	# On Windows/MinGW we need to use 'thread_win32' instead of 'thread'.
	# The library is named libboost_thread_win32* (not libboost_thread*).
	find_package(Boost 1.42 COMPONENTS filesystem system thread_win32 unit_test_framework REQUIRED)
else()
	find_package(Boost 1.42 COMPONENTS filesystem system thread unit_test_framework REQUIRED)
endif()

# Find the platform's thread library (needed for C++11 threads).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads REQUIRED)

find_program(QT_QMAKE_EXECUTABLE NAMES qmake4 qmake-qt4 qmake-mac)
find_package(Qt4 REQUIRED)

set(pulseview_TEST_SOURCES
	${PROJECT_SOURCE_DIR}/pv/devicemanager.cpp
	${PROJECT_SOURCE_DIR}/pv/sigsession.cpp
	${PROJECT_SOURCE_DIR}/pv/util.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursorpair.cpp
	${PROJECT_SOURCE_DIR}/pv/data/analog.cpp
	${PROJECT_SOURCE_DIR}/pv/data/analogsnapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logic.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logicsnapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/snapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/signaldata.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/double.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/enum.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/int.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/property.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/string.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/binding/binding.cpp
	${PROJECT_SOURCE_DIR}/pv/view/analogsignal.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursor.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursorheader.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursorpair.cpp
	${PROJECT_SOURCE_DIR}/pv/view/header.cpp
	${PROJECT_SOURCE_DIR}/pv/view/logicsignal.cpp
	${PROJECT_SOURCE_DIR}/pv/view/marginwidget.cpp
	${PROJECT_SOURCE_DIR}/pv/view/ruler.cpp
	${PROJECT_SOURCE_DIR}/pv/view/selectableitem.cpp
	${PROJECT_SOURCE_DIR}/pv/view/signal.cpp
	${PROJECT_SOURCE_DIR}/pv/view/timemarker.cpp
	${PROJECT_SOURCE_DIR}/pv/view/trace.cpp
	${PROJECT_SOURCE_DIR}/pv/view/tracepalette.cpp
	${PROJECT_SOURCE_DIR}/pv/view/view.cpp
	${PROJECT_SOURCE_DIR}/pv/view/viewport.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colourbutton.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colourpopup.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popup.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/wellarray.cpp
	data/analogsnapshot.cpp
	data/logicsnapshot.cpp
	test.cpp
)

# This list includes only QObject derived class headers.
set(pulseview_TEST_HEADERS
	${PROJECT_SOURCE_DIR}/pv/sigsession.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/double.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/enum.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/int.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/property.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/string.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/binding/deviceoptions.hpp
	${PROJECT_SOURCE_DIR}/pv/view/cursor.hpp
	${PROJECT_SOURCE_DIR}/pv/view/cursorheader.hpp
	${PROJECT_SOURCE_DIR}/pv/view/header.hpp
	${PROJECT_SOURCE_DIR}/pv/view/logicsignal.hpp
	${PROJECT_SOURCE_DIR}/pv/view/marginwidget.hpp
	${PROJECT_SOURCE_DIR}/pv/view/ruler.hpp
	${PROJECT_SOURCE_DIR}/pv/view/selectableitem.hpp
	${PROJECT_SOURCE_DIR}/pv/view/signal.hpp
	${PROJECT_SOURCE_DIR}/pv/view/timemarker.hpp
	${PROJECT_SOURCE_DIR}/pv/view/trace.hpp
	${PROJECT_SOURCE_DIR}/pv/view/view.hpp
	${PROJECT_SOURCE_DIR}/pv/view/viewport.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colourbutton.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colourpopup.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popup.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/wellarray.hpp
)

if(ENABLE_DECODE)
	list(APPEND pulseview_TEST_SOURCES
		${PROJECT_SOURCE_DIR}/pv/data/decoderstack.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/annotation.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/decoder.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/row.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/rowdata.cpp
		${PROJECT_SOURCE_DIR}/pv/prop/binding/decoderoptions.cpp
		${PROJECT_SOURCE_DIR}/pv/view/decodetrace.cpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodergroupbox.cpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodermenu.cpp
		data/decoderstack.cpp
	)

	list(APPEND pulseview_TEST_HEADERS
		${PROJECT_SOURCE_DIR}/pv/data/decoderstack.hpp
		${PROJECT_SOURCE_DIR}/pv/view/decodetrace.hpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodergroupbox.hpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodermenu.hpp
	)
endif()

qt4_wrap_cpp(pulseview_TEST_HEADERS_MOC ${pulseview_TEST_HEADERS})

if(ENABLE_DECODE)
	add_definitions(-DENABLE_DECODE)
endif()

# On MinGW we need to use static linking.
if(NOT WIN32)
	add_definitions(-DBOOST_TEST_DYN_LINK)
endif()

add_definitions(${QT_DEFINITIONS})

include_directories(
	${Boost_INCLUDE_DIRS}
	${PKGDEPS_INCLUDE_DIRS}
)

set(PULSEVIEW_LINK_LIBS
	${Boost_LIBRARIES}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)

if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()

add_executable(pulseview-test
	${pulseview_TEST_SOURCES}
	${pulseview_TEST_HEADERS_MOC}
)

target_link_libraries(pulseview-test ${PULSEVIEW_LINK_LIBS})

