cmake_minimum_required(VERSION 2.6)
include(FindPkgConfig)

project(sigrok-qt2)

find_package(PkgConfig)
pkg_check_modules(PKGDEPS REQUIRED
	libsigrokdecode>=0.1.0
	libsigrok>=0.1.0
)

find_package(Qt4 REQUIRED)
find_package(Boost 1.46 REQUIRED)

set(VERSION 0.1.0)

set(sigrok-qt2_SOURCES
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
	sigview.cpp
)

set(sigrok-qt2_HEADERS
	about.h
	mainwindow.h
	samplingbar.h
	sigsession.h
	sigview.h
)

set(sigrok-qt2_FORMS
	about.ui
)

set(sigrok-qt2_RESOURCES
	sigrok-qt2.qrc
)

set(QT_USE_QTOPENGL TRUE)

qt4_wrap_cpp(sigrok-qt2_HEADERS_MOC ${sigrok-qt2_HEADERS})
qt4_wrap_ui(sigrok-qt2_FORMS_HEADERS ${sigrok-qt2_FORMS})
qt4_add_resources(sigrok-qt2_RESOURCES_RCC ${sigrok-qt2_RESOURCES})

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

add_executable(sigrok-qt2
	${sigrok-qt2_SOURCES}
	${sigrok-qt2_HEADERS_MOC}
	${sigrok-qt2_FORMS_HEADERS}
	${sigrok-qt2_RESOURCES_RCC}
)

target_link_libraries(sigrok-qt2
	${Boost_LIBRARIES}
	${PKGDEPS_LIBRARIES}
	${QT_LIBRARIES}
)
