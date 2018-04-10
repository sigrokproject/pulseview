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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "colorpopup.hpp"

namespace pv {
namespace widgets {

ColorPopup::ColorPopup(int rows, int cols, QWidget *parent) :
	Popup(parent),
	well_array_(rows, cols, this),
	layout_(this)
{
	layout_.addWidget(&well_array_);
	setLayout(&layout_);

	connect(&well_array_, SIGNAL(selected(int, int)),
		this, SIGNAL(selected(int, int)));
	connect(&well_array_, SIGNAL(selected(int, int)),
		this, SLOT(color_selected(int, int)));
}

WellArray& ColorPopup::well_array()
{
	return well_array_;
}

void ColorPopup::color_selected(int, int)
{
	close();
}

}  // namespace widgets
}  // namespace pv
