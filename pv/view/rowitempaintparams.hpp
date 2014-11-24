/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_VIEW_ROWITEMPAINTPARAMS_H
#define PULSEVIEW_PV_VIEW_ROWITEMPAINTPARAMS_H

#include <QFont>

namespace pv {
namespace view {

class RowItemPaintParams
{
public:
	RowItemPaintParams(int left, int right, double scale, double offset);

	int left() const {
		return left_;
	}

	int right() const {
		return right_;
	}

	double scale() const {
		return scale_;
	}

	double offset() const {
		return offset_;
	}

	int width() const {
		return right_ - left_;
	}

	double pixels_offset() const {
		return offset_ / scale_;
	}

public:
	static QFont font();

	static int text_height();

private:
	int left_;
	int right_;
	double scale_;
	double offset_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_ROWITEMPAINTPARAMS_H
