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

#include <cassert>

#include <QApplication>
#include <QFontMetrics>

#include "rowitempaintparams.hpp"

namespace pv {
namespace view {

RowItemPaintParams::RowItemPaintParams(
	int left, int right, double scale, double offset) :
	left_(left),
	right_(right),
	scale_(scale),
	offset_(offset) {
	assert(left <= right);
	assert(scale > 0.0);
}

QFont RowItemPaintParams::font()
{
	return QApplication::font();
}

int RowItemPaintParams::text_height() {
	QFontMetrics m(font());
	return m.boundingRect(QRect(), 0, "Tg").height();
}

} // namespace view
} // namespace pv
