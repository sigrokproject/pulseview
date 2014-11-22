/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "tracepalette.hpp"

namespace pv {
namespace view {

const QColor TracePalette::Colours[Cols * Rows] = {

	// Light Colours
	QColor(0xFC, 0xE9, 0x4F),	// Butter
	QColor(0xFC, 0xAF, 0x3E),	// Orange
	QColor(0xE9, 0xB9, 0x6E),	// Chocolate
	QColor(0x8A, 0xE2, 0x34),	// Chameleon
	QColor(0x72, 0x9F, 0xCF),	// Sky Blue
	QColor(0xAD, 0x7F, 0xA8),	// Plum
	QColor(0xCF, 0x72, 0xC3),	// Magenta
	QColor(0xEF, 0x29, 0x29),	// Scarlet Red

	// Mid Colours
	QColor(0xED, 0xD4, 0x00),	// Butter
	QColor(0xF5, 0x79, 0x00),	// Orange
	QColor(0xC1, 0x7D, 0x11),	// Chocolate
	QColor(0x73, 0xD2, 0x16),	// Chameleon
	QColor(0x34, 0x65, 0xA4),	// Sky Blue
	QColor(0x75, 0x50, 0x7B),	// Plum
	QColor(0xA3, 0x34, 0x96),	// Magenta
	QColor(0xCC, 0x00, 0x00),	// Scarlet Red

	// Dark Colours
	QColor(0xC4, 0xA0, 0x00),	// Butter
	QColor(0xCE, 0x5C, 0x00),	// Orange
	QColor(0x8F, 0x59, 0x02),	// Chocolate
	QColor(0x4E, 0x9A, 0x06),	// Chameleon
	QColor(0x20, 0x4A, 0x87),	// Sky Blue
	QColor(0x5C, 0x35, 0x66),	// Plum
	QColor(0x87, 0x20, 0x7A),	// Magenta
	QColor(0xA4, 0x00, 0x00),	// Scarlet Red

	// Greys
	QColor(0x16, 0x19, 0x1A),	// Black
	QColor(0x2E, 0x34, 0x36),	// Grey 1
	QColor(0x55, 0x57, 0x53),	// Grey 2
	QColor(0x88, 0x8A, 0x8F),	// Grey 3
	QColor(0xBA, 0xBD, 0xB6),	// Grey 4
	QColor(0xD3, 0xD7, 0xCF),	// Grey 5
	QColor(0xEE, 0xEE, 0xEC),	// Grey 6
	QColor(0xFF, 0xFF, 0xFF),	// White
};

} // view
} // pv
