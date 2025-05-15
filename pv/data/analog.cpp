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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>

#include "analog.hpp"
#include "analogsegment.hpp"

using std::deque;
using std::max;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace data {

const QColor Analog::SignalColors[8] =
{
	QColor(0xC4, 0xA0, 0x00),	// Yellow   HSV:  49 / 100 / 77
	QColor(0x87, 0x20, 0x7A),	// Magenta  HSV: 308 /  70 / 53
	QColor(0x20, 0x4A, 0x87),	// Blue     HSV: 216 /  76 / 53
	QColor(0x4E, 0x9A, 0x06),	// Green    HSV:  91 /  96 / 60
	QColor(0xBF, 0x6E, 0x00),	// Orange   HSV:  35 / 100 / 75
	QColor(0x5E, 0x20, 0x80),	// Purple   HSV: 280 /  75 / 50
	QColor(0x20, 0x80, 0x7A),	// Turqoise HSV: 177 /  75 / 50
	QColor(0x80, 0x20, 0x24)	// Red      HSV: 358 /  75 / 50
};

const char *Analog::InvalidSignal = "\"%1\" isn't a valid analog signal";

//const SignalBase::ChannelType math_channel_type = SignalBase::AnalogMathChannel;

Analog::Analog() :
	SignalData(),
	samplerate_(1)  // Default is 1 Hz to prevent division-by-zero errors
{
}

void Analog::push_segment(shared_ptr<AnalogSegment> &segment)
{
	segments_.push_back(segment);

	if ((samplerate_ == 1) && (segment->samplerate() > 1))
		samplerate_ = segment->samplerate();

	connect(segment.get(), SIGNAL(completed()), this, SLOT(on_segment_completed()));
}

const deque< shared_ptr<AnalogSegment> >& Analog::analog_segments() const
{
	return segments_;
}

vector< shared_ptr<Segment> > Analog::segments() const
{
	return vector< shared_ptr<Segment> >(
		segments_.begin(), segments_.end());
}

uint32_t Analog::get_segment_count() const
{
	return (uint32_t)segments_.size();
}

void Analog::clear()
{
	if (!segments_.empty()) {
		segments_.clear();

		samples_cleared();
	}
}

void Analog::set_samplerate(double value)
{
	samplerate_ = value;
}

double Analog::get_samplerate() const
{
	return samplerate_;
}

uint64_t Analog::max_sample_count() const
{
	uint64_t l = 0;
	for (const shared_ptr<AnalogSegment>& s : segments_) {
		assert(s);
		l = max(l, s->get_sample_count());
	}
	return l;
}

void Analog::notify_samples_added(shared_ptr<Segment> segment, uint64_t start_sample,
	uint64_t end_sample)
{
	samples_added(segment, start_sample, end_sample);
}

void Analog::notify_min_max_changed(float min, float max)
{
	min_max_changed(min, max);
}

void Analog::on_segment_completed()
{
	segment_completed();
}

} // namespace data
} // namespace pv
