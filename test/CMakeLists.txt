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

list(APPEND PKGDEPS
	libsigrok>=0.2.0
	libsigrokdecode>=0.2.0
)

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

# Find the platform's thread library (needed for boost-thread).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads)

find_package(Boost 1.42 COMPONENTS system thread unit_test_framework REQUIRED)

FIND_PROGRAM(QT_QMAKE_EXECUTABLE NAMES qmake4 qmake-qt4 qmake-mac)
find_package(Qt4 REQUIRED)

set(pulseview_TEST_SOURCES
	${PROJECT_SOURCE_DIR}/pv/devicemanager.cpp
	${PROJECT_SOURCE_DIR}/pv/sigsession.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursorpair.cpp
	${PROJECT_SOURCE_DIR}/pv/data/analog.cpp
	${PROJECT_SOURCE_DIR}/pv/data/analogsnapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/decoder.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logic.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logicsnapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/snapshot.cpp
	${PROJECT_SOURCE_DIR}/pv/data/signaldata.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/int.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/property.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/string.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/binding/binding.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/binding/decoderoptions.cpp
	${PROJECT_SOURCE_DIR}/pv/view/analogsignal.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursor.cpp
	${PROJECT_SOURCE_DIR}/pv/view/cursorpair.cpp
	${PROJECT_SOURCE_DIR}/pv/view/decodetrace.cpp
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
	${PROJECT_SOURCE_DIR}/pv/view/decode/annotation.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colourbutton.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colourpopup.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popup.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/wellarray.cpp
	data/analogsnapshot.cpp
	data/decoder.cpp
	data/logicsnapshot.cpp
	test.cpp
)

# This list includes only QObject derrived class headers
set(pulseview_TEST_HEADERS
	${PROJECT_SOURCE_DIR}/pv/sigsession.h
	${PROJECT_SOURCE_DIR}/pv/data/decoder.h
	${PROJECT_SOURCE_DIR}/pv/prop/int.h
	${PROJECT_SOURCE_DIR}/pv/prop/property.h
	${PROJECT_SOURCE_DIR}/pv/prop/string.h
	${PROJECT_SOURCE_DIR}/pv/view/cursor.h
	${PROJECT_SOURCE_DIR}/pv/view/decodetrace.h
	${PROJECT_SOURCE_DIR}/pv/view/header.h
	${PROJECT_SOURCE_DIR}/pv/view/logicsignal.h
	${PROJECT_SOURCE_DIR}/pv/view/marginwidget.h
	${PROJECT_SOURCE_DIR}/pv/view/ruler.h
	${PROJECT_SOURCE_DIR}/pv/view/selectableitem.h
	${PROJECT_SOURCE_DIR}/pv/view/signal.h
	${PROJECT_SOURCE_DIR}/pv/view/timemarker.h
	${PROJECT_SOURCE_DIR}/pv/view/trace.h
	${PROJECT_SOURCE_DIR}/pv/view/view.h
	${PROJECT_SOURCE_DIR}/pv/view/viewport.h
	${PROJECT_SOURCE_DIR}/pv/widgets/colourbutton.h
	${PROJECT_SOURCE_DIR}/pv/widgets/colourpopup.h
	${PROJECT_SOURCE_DIR}/pv/widgets/wellarray.h
)

qt4_wrap_cpp(pulseview_TEST_HEADERS_MOC ${pulseview_TEST_HEADERS})

add_definitions(-DBOOST_TEST_DYN_LINK)
add_definitions(${QT_DEFINITIONS})

include_directories(
	${Boost_INCLUDE_DIRS}
	${PKGDEPS_INCLUDE_DIRS}
)

set(PULSEVIEW_LINK_LIBS
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
)

add_executable(pulseview-test
	${pulseview_TEST_SOURCES}
	${pulseview_TEST_HEADERS_MOC}
)

target_link_libraries(pulseview-test ${PULSEVIEW_LINK_LIBS})

