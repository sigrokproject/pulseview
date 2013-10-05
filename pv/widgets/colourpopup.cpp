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

#include "colourpopup.h"

namespace pv {
namespace widgets {

ColourPopup::ColourPopup(int rows, int cols, QWidget *parent) :
	Popup(parent),
	_well_array(rows, cols, this),
	_layout(this)
{
	_layout.addWidget(&_well_array);
	setLayout(&_layout);

	connect(&_well_array, SIGNAL(selected(int, int)),
		this, SIGNAL(selected(int, int)));
	connect(&_well_array, SIGNAL(selected(int, int)),
		this, SLOT(colour_selected(int, int)));
}

QWellArray& ColourPopup::well_array()
{
	return _well_array;
}

void ColourPopup::colour_selected(int, int)
{
	close();
}

} // widgets
} // pv
