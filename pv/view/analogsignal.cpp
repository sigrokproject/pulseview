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
#include "pv/data/analog.h"
#include "pv/data/analogsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

AnalogSignal::AnalogSignal(QString name, shared_ptr<data::Analog> data) :
	Signal(name),
	_data(data)
{
	_colour = Qt::blue;
}

void AnalogSignal::paint(QPainter &p, const QRect &rect, double scale,
	double offset)
{
	assert(scale > 0);
	assert(_data);

	const deque< shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::AnalogSnapshot> &snapshot =
		snapshots.front();

	const double pixels_offset = offset / scale;
	const double samplerate = _data->get_samplerate();
	const double start_time = _data->get_start_time();
	const int64_t last_sample = snapshot->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * scale;
	const double start = samplerate * (offset - start_time);
	const double end = start + samples_per_pixel * rect.width();

	const int64_t start_sample = min(max((int64_t)floor(start),
		(int64_t)0), last_sample);
	const int64_t end_sample = min(max((int64_t)ceil(end),
		(int64_t)0), last_sample);

	const int y_offset = rect.center().y();

	const float* samples = snapshot->get_samples();
	assert(samples);

	QPointF *points = new QPointF[end_sample - start_sample];
	QPointF *point = points;

	for (int64_t sample = start_sample;
		sample != end_sample; sample++) {
		const float x = (sample / samples_per_pixel -
			pixels_offset) + rect.left();
		const float y = samples[sample] + y_offset;
		*point++ = QPointF(x, y);
	}

	p.drawPoints(points, point - points);

	delete[] points;
}

int AnalogSignal::get_nominal_offset(const QRect &rect) const
{
	return rect.center().y();
}

} // namespace view
} // namespace pv
