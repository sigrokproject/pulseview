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

set(pulseview_TEST_SOURCES
	${PROJECT_SOURCE_DIR}/pv/application.cpp
	${PROJECT_SOURCE_DIR}/pv/devicemanager.cpp
	${PROJECT_SOURCE_DIR}/pv/globalsettings.cpp
	${PROJECT_SOURCE_DIR}/pv/logging.cpp
	${PROJECT_SOURCE_DIR}/pv/mainwindow.cpp
	${PROJECT_SOURCE_DIR}/pv/metadata_obj.cpp
	${PROJECT_SOURCE_DIR}/pv/session.cpp
	${PROJECT_SOURCE_DIR}/pv/storesession.cpp
	${PROJECT_SOURCE_DIR}/pv/util.cpp
	${PROJECT_SOURCE_DIR}/pv/binding/binding.cpp
	${PROJECT_SOURCE_DIR}/pv/binding/device.cpp
	${PROJECT_SOURCE_DIR}/pv/binding/inputoutput.cpp
	${PROJECT_SOURCE_DIR}/pv/data/analog.cpp
	${PROJECT_SOURCE_DIR}/pv/data/analogsegment.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logic.cpp
	${PROJECT_SOURCE_DIR}/pv/data/logicsegment.cpp
	${PROJECT_SOURCE_DIR}/pv/data/mathsignal.cpp
	${PROJECT_SOURCE_DIR}/pv/data/segment.cpp
	${PROJECT_SOURCE_DIR}/pv/data/signalbase.cpp
	${PROJECT_SOURCE_DIR}/pv/data/signaldata.cpp
	${PROJECT_SOURCE_DIR}/pv/devices/device.cpp
	${PROJECT_SOURCE_DIR}/pv/devices/file.cpp
	${PROJECT_SOURCE_DIR}/pv/devices/hardwaredevice.cpp
	${PROJECT_SOURCE_DIR}/pv/devices/inputfile.cpp
	${PROJECT_SOURCE_DIR}/pv/devices/sessionfile.cpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/connect.cpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/inputoutputoptions.cpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/settings.cpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/storeprogress.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/bool.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/double.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/enum.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/int.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/property.cpp
	${PROJECT_SOURCE_DIR}/pv/prop/string.cpp
	${PROJECT_SOURCE_DIR}/pv/popups/channels.cpp
	${PROJECT_SOURCE_DIR}/pv/popups/deviceoptions.cpp
	${PROJECT_SOURCE_DIR}/pv/subwindows/subwindowbase.cpp
	${PROJECT_SOURCE_DIR}/pv/toolbars/mainbar.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/analogsignal.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/cursor.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/cursorpair.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/flag.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/header.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/marginwidget.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/logicsignal.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/ruler.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/signal.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/timeitem.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/timemarker.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/trace.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/tracegroup.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/tracepalette.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/tracetreeitem.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/tracetreeitemowner.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/triggermarker.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/view.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewitem.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewitemowner.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewitempaintparams.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewport.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewwidget.cpp
	${PROJECT_SOURCE_DIR}/pv/views/viewbase.cpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/standardbar.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colorbutton.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colorpopup.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/devicetoolbutton.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/exportmenu.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/flowlayout.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/importmenu.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popup.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popuptoolbutton.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/sweeptimingwidget.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/timestampspinbox.cpp
	${PROJECT_SOURCE_DIR}/pv/widgets/wellarray.cpp
	data/analogsegment.cpp
	data/logicsegment.cpp
	data/segment.cpp
	view/ruler.cpp
	test.cpp
	util.cpp
)

# This list includes only QObject derived class headers.
set(pulseview_TEST_HEADERS
	${PROJECT_SOURCE_DIR}/pv/application.hpp
	${PROJECT_SOURCE_DIR}/pv/devicemanager.hpp
	${PROJECT_SOURCE_DIR}/pv/globalsettings.hpp
	${PROJECT_SOURCE_DIR}/pv/logging.hpp
	${PROJECT_SOURCE_DIR}/pv/mainwindow.hpp
	${PROJECT_SOURCE_DIR}/pv/metadata_obj.hpp
	${PROJECT_SOURCE_DIR}/pv/session.hpp
	${PROJECT_SOURCE_DIR}/pv/storesession.hpp
	${PROJECT_SOURCE_DIR}/pv/binding/device.hpp
	${PROJECT_SOURCE_DIR}/pv/data/analog.hpp
	${PROJECT_SOURCE_DIR}/pv/data/analogsegment.hpp
	${PROJECT_SOURCE_DIR}/pv/data/logic.hpp
	${PROJECT_SOURCE_DIR}/pv/data/logicsegment.hpp
	${PROJECT_SOURCE_DIR}/pv/data/mathsignal.hpp
	${PROJECT_SOURCE_DIR}/pv/data/signalbase.hpp
	${PROJECT_SOURCE_DIR}/pv/devices/device.hpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/connect.hpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/inputoutputoptions.hpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/settings.hpp
	${PROJECT_SOURCE_DIR}/pv/dialogs/storeprogress.hpp
	${PROJECT_SOURCE_DIR}/pv/popups/channels.hpp
	${PROJECT_SOURCE_DIR}/pv/popups/deviceoptions.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/bool.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/double.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/enum.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/int.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/property.hpp
	${PROJECT_SOURCE_DIR}/pv/prop/string.hpp
	${PROJECT_SOURCE_DIR}/pv/subwindows/subwindowbase.hpp
	${PROJECT_SOURCE_DIR}/pv/toolbars/mainbar.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/analogsignal.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/cursor.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/flag.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/header.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/logicsignal.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/marginwidget.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/ruler.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/signal.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/timeitem.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/timemarker.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/trace.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/tracegroup.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/tracetreeitem.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/triggermarker.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/view.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewitem.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewport.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/viewwidget.hpp
	${PROJECT_SOURCE_DIR}/pv/views/viewbase.hpp
	${PROJECT_SOURCE_DIR}/pv/views/trace/standardbar.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colorbutton.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/colorpopup.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/devicetoolbutton.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/exportmenu.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/flowlayout.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/importmenu.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popup.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/popuptoolbutton.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/sweeptimingwidget.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/timestampspinbox.hpp
	${PROJECT_SOURCE_DIR}/pv/widgets/wellarray.hpp
)

if(ENABLE_DECODE)
	list(APPEND pulseview_TEST_SOURCES
		${PROJECT_SOURCE_DIR}/pv/binding/decoder.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decodesignal.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/annotation.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/decoder.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/row.cpp
		${PROJECT_SOURCE_DIR}/pv/data/decode/rowdata.cpp
		${PROJECT_SOURCE_DIR}/pv/subwindows/decoder_selector/item.cpp
		${PROJECT_SOURCE_DIR}/pv/subwindows/decoder_selector/model.cpp
		${PROJECT_SOURCE_DIR}/pv/subwindows/decoder_selector/subwindow.cpp
		${PROJECT_SOURCE_DIR}/pv/views/decoder_binary/view.cpp
		${PROJECT_SOURCE_DIR}/pv/views/decoder_binary/QHexView.cpp
		${PROJECT_SOURCE_DIR}/pv/views/tabular_decoder/model.cpp
		${PROJECT_SOURCE_DIR}/pv/views/tabular_decoder/view.cpp
		${PROJECT_SOURCE_DIR}/pv/views/trace/decodetrace.cpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodergroupbox.cpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodermenu.cpp
	)

	list(APPEND pulseview_TEST_HEADERS
		${PROJECT_SOURCE_DIR}/pv/data/decodesignal.hpp
		${PROJECT_SOURCE_DIR}/pv/subwindows/decoder_selector/subwindow.hpp
		${PROJECT_SOURCE_DIR}/pv/views/decoder_binary/view.hpp
		${PROJECT_SOURCE_DIR}/pv/views/decoder_binary/QHexView.hpp
		${PROJECT_SOURCE_DIR}/pv/views/tabular_decoder/view.hpp
		${PROJECT_SOURCE_DIR}/pv/views/trace/decodetrace.hpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodergroupbox.hpp
		${PROJECT_SOURCE_DIR}/pv/widgets/decodermenu.hpp
	)
endif()

# On MinGW we need to use static linking.
if(NOT WIN32)
	add_definitions(-DBOOST_TEST_DYN_LINK)
endif()

add_executable(pulseview-test
	${pulseview_TEST_SOURCES}
	${pulseview_TEST_HEADERS_MOC}
)

target_link_libraries(pulseview-test ${PULSEVIEW_LINK_LIBS})

