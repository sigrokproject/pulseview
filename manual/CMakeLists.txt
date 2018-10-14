##
## This file is part of the PulseView project.
##
## Copyright (C) 2018 Gerhard Sittig <gerhard.sittig@gmx.net>
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

find_program(ASCIIDOCTOR_EXECUTABLE NAMES asciidoctor)
find_program(ASCIIDOCTOR_PDF_EXECUTABLE NAMES asciidoctor-pdf)

set(STYLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asciidoctor-stylesheet-factory/stylesheets")
set(STYLE_SHEET "readthedocs.css")
set(MANUAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/pulseview_manual.txt")
set(MANUAL_OUT_HTML "${CMAKE_CURRENT_BINARY_DIR}/pulseview_manual.html")
set(MANUAL_OUT_PDF "${CMAKE_CURRENT_BINARY_DIR}/pulseview_manual.pdf")

add_custom_target(manual-html
	COMMAND ${ASCIIDOCTOR_EXECUTABLE}
		-a stylesheet=${STYLE_SHEET}
		-a stylesdir=${STYLES_DIR}
		-a toc=left
		--destination-dir=${CMAKE_CURRENT_BINARY_DIR}
		${MANUAL_SRC}
	BYPRODUCTS ${MANUAL_OUT_HTML}
	DEPENDS ${MANUAL_SRC}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating manual, HTML output"
)
add_custom_target(manual-pdf
	COMMAND ${ASCIIDOCTOR_PDF_EXECUTABLE}
		-a stylesheet=${STYLE_SHEET}
		-a stylesdir=${STYLES_DIR}
		--destination-dir=${CMAKE_CURRENT_BINARY_DIR}
		${MANUAL_SRC}
	BYPRODUCTS ${MANUAL_OUT_PDF}
	DEPENDS ${MANUAL_SRC}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating manual, HTML output"
)
add_custom_target(manual)
add_dependencies(manual manual-html manual-pdf)
