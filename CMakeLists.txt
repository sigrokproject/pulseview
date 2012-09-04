cmake_minimum_required(VERSION 2.6)
include(FindPkgConfig)

project(pulseview)

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED
	libsigrokdecode>=0.1.0
	libsigrok>=0.1.0
)

find_package(Qt4 REQUIRED)
find_package(Boost 1.46 COMPONENTS unit_test_framework REQUIRED)

set(VERSION 0.1.0)

set(pulseview_SOURCES
	about.cpp
	datasnapshot.cpp
	logicdata.cpp
	logicdatasnapshot.cpp
	logicsignal.cpp
	main.cpp
	mainwindow.cpp
	samplingbar.cpp
	signaldata.cpp
	sigsession.cpp
	signal.cpp
	sigviewport.cpp
)

set(pulseview_HEADERS
	about.h
	mainwindow.h
	samplingbar.h
	sigsession.h
	sigviewport.h
)

set(pulseview_FORMS
	about.ui
)

set(pulseview_RESOURCES
	pulseview.qrc
)

set(pulseview_TEST_SOURCES
	test/logicdatasnapshot.cpp
	test/test.cpp
	datasnapshot.cpp
	logicdatasnapshot.cpp
)

set(QT_USE_QTOPENGL TRUE)

qt4_wrap_cpp(pulseview_HEADERS_MOC ${pulseview_HEADERS})
qt4_wrap_ui(pulseview_FORMS_HEADERS ${pulseview_FORMS})
qt4_add_resources(pulseview_RESOURCES_RCC ${pulseview_RESOURCES})

include(${QT_USE_FILE})

add_definitions(${QT_DEFINITIONS})
add_definitions(-DAPP_VERSION="${VERSION}")

include_directories(
	${include_directories}
	${Boost_INCLUDE_DIRS}
	${PKGDEPS_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${PKGDEPS_LIBRARY_DIRS}
)

add_executable(pulseview
	${pulseview_SOURCES}
	${pulseview_HEADERS_MOC}
	${pulseview_FORMS_HEADERS}
	${pulseview_RESOURCES_RCC}
)

target_link_libraries(pulseview
	${Boost_LIBRARIES}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(pulseview-test
	${pulseview_TEST_SOURCES}
)

target_link_libraries(pulseview-test
	${Boost_LIBRARIES}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
)

enable_testing()
add_test(test ${CMAKE_CURRENT_BINARY_DIR}/pulseview-test)
