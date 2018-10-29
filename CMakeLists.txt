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

cmake_minimum_required(VERSION 2.8.12)

include(GNUInstallDirs)

project(pulseview)

# Let AUTOMOC and AUTOUIC process GENERATED files.
if(POLICY CMP0071)
	cmake_policy(SET CMP0071 NEW)
endif()

# Only interpret if() arguments as variables or keywords when unquoted.
if(POLICY CMP0054)
	cmake_policy(SET CMP0054 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

option(DISABLE_WERROR "Build without -Werror" TRUE)
option(ENABLE_SIGNALS "Build with UNIX signals" TRUE)
option(ENABLE_STACKTRACE "Enable stack trace when crashing" FALSE)
option(ENABLE_DECODE "Build with libsigrokdecode" TRUE)
option(ENABLE_TESTS "Enable unit tests" FALSE)
option(STATIC_PKGDEPS_LIBS "Statically link to (pkg-config) libraries" FALSE)

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# Windows does not support UNIX signals.
	set(ENABLE_SIGNALS FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
	"Choose the type of build (None, Debug, Release, RelWithDebInfo, MinSizeRel)."
	FORCE)
endif()

#===============================================================================
#= Documentation
#-------------------------------------------------------------------------------

add_subdirectory(manual)

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

list(APPEND PKGDEPS glib-2.0>=2.28.0)
list(APPEND PKGDEPS glibmm-2.4>=2.28.0)

set(LIBSR_CXX_BINDING "libsigrokcxx>=0.5.1")
list(APPEND PKGDEPS "${LIBSR_CXX_BINDING}")

if(ENABLE_DECODE)
	list(APPEND PKGDEPS libsigrokdecode>=0.5.2)
endif()

if(ANDROID)
	list(APPEND PKGDEPS libsigrokandroidutils>=0.1.0)
endif()

find_package(PkgConfig)
pkg_check_modules(LIBSRCXX QUIET ${LIBSR_CXX_BINDING})
if(NOT LIBSRCXX_FOUND OR NOT LIBSRCXX_VERSION)
	message(FATAL_ERROR "libsigrok C++ bindings missing, check libsigrok's 'configure' output (missing dependencies?)")
endif()
pkg_check_modules(PKGDEPS REQUIRED ${PKGDEPS})

set(CMAKE_AUTOMOC TRUE)

find_package(Qt5 COMPONENTS Core Gui Widgets Svg REQUIRED)

if(WIN32)
	# MXE workaround: Use pkg-config to find Qt5 libs.
	# https://github.com/mxe/mxe/issues/1642
	# Not required (and doesn't work) on MSYS2.
	if(NOT DEFINED ENV{MSYSTEM})
		pkg_check_modules(QT5ALL REQUIRED Qt5Widgets Qt5Gui Qt5Svg)
	endif()
endif()

set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::Svg)

set(BOOSTCOMPS filesystem serialization system)
if(ENABLE_TESTS)
	list(APPEND BOOSTCOMPS unit_test_framework)
endif()

if(ENABLE_STACKTRACE)
	find_package(Boost 1.65.1 COMPONENTS ${BOOSTCOMPS} REQUIRED)
else()
	find_package(Boost 1.55 COMPONENTS ${BOOSTCOMPS} REQUIRED)
endif()

# Find the platform's thread library (needed for C++11 threads).
# This will set ${CMAKE_THREAD_LIBS_INIT} to the correct, OS-specific value.
find_package(Threads REQUIRED)

# Check for explicit link against libatomic
#
# Depending on the toolchain, linking a program using atomic functions may need
# "-latomic" explicitly passed to the linker
#
# This check first tests if atomics are available in the C-library, if not and
# libatomic exists, then it runs the same test with -latomic added to the
# linker flags.

# Helper for checking for atomics
function(check_working_cxx_atomics varname additional_lib)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "-std=c++11")
  set(CMAKE_REQUIRED_LIBRARIES "${additional_lib}")
  set(CMAKE_REQUIRED_QUIET 1)
  CHECK_CXX_SOURCE_COMPILES("
#include <atomic>
std::atomic<int> x;
int main() {
  return std::atomic_fetch_add_explicit(&x, 1, std::memory_order_seq_cst);
}
" ${varname})
  cmake_pop_check_state()
endfunction(check_working_cxx_atomics)

# First check if atomics work without the library.
# If not, check if the library exists, and atomics work with it.
check_working_cxx_atomics(HAVE_CXX_ATOMICS_WITHOUT_LIB "")
if(HAVE_CXX_ATOMICS_WITHOUT_LIB)
  message(STATUS "Atomics provided by the C-library - yes")
else()
  message(STATUS "Atomics provided by the C-library - no")
  find_library(LIBATOMIC_LIBRARY NAMES atomic PATH_SUFFIXES lib)
  if(LIBATOMIC_LIBRARY)
    check_working_cxx_atomics(HAVE_CXX_ATOMICS_WITH_LIB "${LIBATOMIC_LIBRARY}")
    if (HAVE_CXX_ATOMICS_WITH_LIB)
      message(STATUS "Atomics provided by libatomic - yes")
    else()
      message(STATUS "Atomics provided by libatomic - no")
      message(FATAL_ERROR "Compiler must support std::atomic!")
    endif()
  else()
    message(FATAL_ERROR "Compiler appears to require libatomic, but cannot find it.")
  endif()
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
set(PV_VERSION_STRING "0.4.1")

set(PV_GLIBMM_VERSION ${PKGDEPS_glibmm-2.4_VERSION})

include(GetGitRevisionDescription)

# Append the revision hash unless we are exactly on a tagged release.
git_describe(PV_TAG_VERSION_STRING --match "pulseview-${PV_VERSION_STRING}" --exact-match)
if(NOT PV_TAG_VERSION_STRING)
	get_git_head_revision(PV_REVSPEC PV_HASH)
	if(PV_HASH)
		string(SUBSTRING "${PV_HASH}" 0 7 PV_SHORTHASH)
		set(PV_VERSION_STRING "${PV_VERSION_STRING}-git-${PV_SHORTHASH}")
	endif()

	# Non-tagged releases use the unstable manual
	set(PV_MANUAL_VERSION "unstable")
else()
	# Tagged releases use a fixed manual version
	set(PV_MANUAL_VERSION ${PV_VERSION_STRING})
endif()

if(PV_VERSION_STRING MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[-0-9a-z]*)?$")
	set(PV_VERSION_MAJOR ${CMAKE_MATCH_1})
	set(PV_VERSION_MINOR ${CMAKE_MATCH_2})
	set(PV_VERSION_MICRO ${CMAKE_MATCH_3})
	set(PV_VERSION_SUFFIX ${CMAKE_MATCH_4})
endif()

message(STATUS "${PV_TITLE} version: ${PV_VERSION_STRING}")

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
	pv/globalsettings.cpp
	pv/logging.cpp
	pv/mainwindow.cpp
	pv/session.cpp
	pv/storesession.cpp
	pv/util.cpp
	pv/binding/binding.cpp
	pv/binding/inputoutput.cpp
	pv/binding/device.cpp
	pv/data/analog.cpp
	pv/data/analogsegment.cpp
	pv/data/logic.cpp
	pv/data/logicsegment.cpp
	pv/data/signalbase.cpp
	pv/data/signaldata.cpp
	pv/data/segment.cpp
	pv/devices/device.cpp
	pv/devices/file.cpp
	pv/devices/hardwaredevice.cpp
	pv/devices/inputfile.cpp
	pv/devices/sessionfile.cpp
	pv/dialogs/connect.cpp
	pv/dialogs/inputoutputoptions.cpp
	pv/dialogs/settings.cpp
	pv/dialogs/storeprogress.cpp
	pv/popups/deviceoptions.cpp
	pv/popups/channels.cpp
	pv/prop/bool.cpp
	pv/prop/double.cpp
	pv/prop/enum.cpp
	pv/prop/int.cpp
	pv/prop/property.cpp
	pv/prop/string.cpp
	pv/toolbars/mainbar.cpp
	pv/views/trace/analogsignal.cpp
	pv/views/trace/cursor.cpp
	pv/views/trace/cursorpair.cpp
	pv/views/trace/flag.cpp
	pv/views/trace/header.cpp
	pv/views/trace/marginwidget.cpp
	pv/views/trace/logicsignal.cpp
	pv/views/trace/rowitem.cpp
	pv/views/trace/ruler.cpp
	pv/views/trace/signal.cpp
	pv/views/trace/timeitem.cpp
	pv/views/trace/timemarker.cpp
	pv/views/trace/trace.cpp
	pv/views/trace/tracegroup.cpp
	pv/views/trace/tracepalette.cpp
	pv/views/trace/tracetreeitem.cpp
	pv/views/trace/tracetreeitemowner.cpp
	pv/views/trace/triggermarker.cpp
	pv/views/trace/view.cpp
	pv/views/trace/viewitem.cpp
	pv/views/trace/viewitemowner.cpp
	pv/views/trace/viewitempaintparams.cpp
	pv/views/trace/viewport.cpp
	pv/views/trace/viewwidget.cpp
	pv/views/viewbase.cpp
	pv/views/trace/standardbar.cpp
	pv/widgets/colorbutton.cpp
	pv/widgets/colorpopup.cpp
	pv/widgets/devicetoolbutton.cpp
	pv/widgets/exportmenu.cpp
	pv/widgets/importmenu.cpp
	pv/widgets/popup.cpp
	pv/widgets/popuptoolbutton.cpp
	pv/widgets/sweeptimingwidget.cpp
	pv/widgets/timestampspinbox.cpp
	pv/widgets/wellarray.cpp
)

# This list includes only QObject derived class headers.
set(pulseview_HEADERS
	pv/logging.hpp
	pv/globalsettings.hpp
	pv/mainwindow.hpp
	pv/session.hpp
	pv/storesession.hpp
	pv/binding/device.hpp
	pv/data/analog.hpp
	pv/data/analogsegment.hpp
	pv/data/logic.hpp
	pv/data/logicsegment.hpp
	pv/data/signalbase.hpp
	pv/dialogs/connect.hpp
	pv/dialogs/inputoutputoptions.hpp
	pv/dialogs/settings.hpp
	pv/dialogs/storeprogress.hpp
	pv/popups/channels.hpp
	pv/popups/deviceoptions.hpp
	pv/prop/bool.hpp
	pv/prop/double.hpp
	pv/prop/enum.hpp
	pv/prop/int.hpp
	pv/prop/property.hpp
	pv/prop/string.hpp
	pv/toolbars/mainbar.hpp
	pv/views/trace/analogsignal.hpp
	pv/views/trace/cursor.hpp
	pv/views/trace/flag.hpp
	pv/views/trace/header.hpp
	pv/views/trace/logicsignal.hpp
	pv/views/trace/marginwidget.hpp
	pv/views/trace/rowitem.hpp
	pv/views/trace/ruler.hpp
	pv/views/trace/signal.hpp
	pv/views/trace/timeitem.hpp
	pv/views/trace/timemarker.hpp
	pv/views/trace/trace.hpp
	pv/views/trace/tracegroup.hpp
	pv/views/trace/tracetreeitem.hpp
	pv/views/trace/triggermarker.hpp
	pv/views/trace/view.hpp
	pv/views/trace/viewitem.hpp
	pv/views/trace/viewport.hpp
	pv/views/trace/viewwidget.hpp
	pv/views/viewbase.hpp
	pv/views/trace/standardbar.hpp
	pv/widgets/colorbutton.hpp
	pv/widgets/colorpopup.hpp
	pv/widgets/devicetoolbutton.hpp
	pv/widgets/exportmenu.hpp
	pv/widgets/importmenu.hpp
	pv/widgets/popup.hpp
	pv/widgets/popuptoolbutton.hpp
	pv/widgets/sweeptimingwidget.hpp
	pv/widgets/timestampspinbox.hpp
	pv/widgets/wellarray.hpp
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
		pv/binding/decoder.cpp
		pv/data/decodesignal.cpp
		pv/data/decode/annotation.cpp
		pv/data/decode/decoder.cpp
		pv/data/decode/row.cpp
		pv/data/decode/rowdata.cpp
		pv/views/trace/decodetrace.cpp
		pv/widgets/decodergroupbox.cpp
		pv/widgets/decodermenu.cpp
	)

	list(APPEND pulseview_HEADERS
		pv/data/decodesignal.hpp
		pv/views/trace/decodetrace.hpp
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
	list(APPEND pulseview_SOURCES
		android/assetreader.cpp
		android/loghandler.cpp
	)
endif()

qt5_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})

#===============================================================================
#= Global Definitions
#-------------------------------------------------------------------------------

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-Wall -Wextra)
add_definitions(-std=c++11)
add_definitions(-DBOOST_MATH_DISABLE_FLOAT128=1)

if(ENABLE_DECODE)
	add_definitions(-DENABLE_DECODE)
endif()

if(NOT DISABLE_WERROR)
	add_definitions(-Werror)
endif()

if(ENABLE_SIGNALS)
	add_definitions(-DENABLE_SIGNALS)
endif()

if(ENABLE_STACKTRACE)
	add_definitions(-DENABLE_STACKTRACE)
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
	${QT_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${LIBATOMIC_LIBRARY}
)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND PULSEVIEW_LINK_LIBS ${PKGDEPS_STATIC_LDFLAGS})
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND PULSEVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

if(WIN32)
	# On Windows we need to statically link the libqsvg imageformat
	# plugin (and the QtSvg component) for SVG graphics/icons to work.
	# We also need QWindowsIntegrationPlugin, Qt5PlatformSupport (only for
	# Qt < 5.8.0), and all Qt libs and their dependencies.
	add_definitions(-DQT_STATICPLUGIN)
	list(APPEND PULSEVIEW_LINK_LIBS Qt5::QSvgPlugin)
	list(APPEND PULSEVIEW_LINK_LIBS Qt5::QWindowsIntegrationPlugin)
	if(Qt5Gui_VERSION VERSION_LESS 5.8.0)
		list(APPEND PULSEVIEW_LINK_LIBS -lQt5PlatformSupport)
	endif()
	list(APPEND PULSEVIEW_LINK_LIBS ${QT5ALL_LDFLAGS})
endif()

if(ENABLE_STACKTRACE)
	# Needed to resolve dladdr.
	list(APPEND PULSEVIEW_LINK_LIBS "-ldl")
endif()

if(ANDROID)
	list(APPEND PULSEVIEW_LINK_LIBS "-llog")
endif()

if(ANDROID)
	add_library(${PROJECT_NAME} SHARED ${pulseview_SOURCES} ${pulseview_RESOURCES_RCC})
else()
	add_executable(${PROJECT_NAME} ${pulseview_SOURCES} ${pulseview_RESOURCES_RCC})
endif()

target_link_libraries(${PROJECT_NAME} ${PULSEVIEW_LINK_LIBS})

if(WIN32 AND NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	# Pass -mwindows so that no "DOS box" opens when PulseView is started.
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
install(TARGETS ${PROJECT_NAME} DESTINATION bin/)

# Install the manpage.
install(FILES doc/pulseview.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT doc)

# Install the desktop file.
install(FILES contrib/org.sigrok.PulseView.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# Install the AppData/AppStream file.
install(FILES contrib/org.sigrok.PulseView.appdata.xml DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)

# Install the PulseView icons.
install(FILES icons/pulseview.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps)
install(FILES icons/pulseview.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# Generate Windows installer script.
configure_file(contrib/pulseview_cross.nsi.in contrib/pulseview_cross.nsi @ONLY)

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${PV_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PV_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PV_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${PV_VERSION_STRING}")
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
