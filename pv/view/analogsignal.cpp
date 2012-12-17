/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <extdef.h>

#include <math.h>

#include "analogsignal.h"
#include "../analogdata.h"
#include "../analogdatasnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

AnalogSignal::AnalogSignal(QString name, shared_ptr<AnalogData> data) :
	Signal(name),
	_data(data)
{
	_colour = Qt::blue;
}

void AnalogSignal::paint(QPainter &p, const QRect &rect, double scale,
	double offset)
{
}

int AnalogSignal::get_nominal_offset(const QRect &rect) const
{
	return rect.bottom();
}

} // namespace view
} // namespace pv
