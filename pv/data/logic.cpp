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

#include "logic.hpp"
#include "logicsegment.hpp"

using std::deque;
using std::max;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace data {

Logic::Logic(unsigned int num_channels) :
	SignalData(),
	samplerate_(1),  // Default is 1 Hz to prevent division-by-zero errors
	num_channels_(num_channels)
{
	assert(num_channels_ > 0);
}

unsigned int Logic::num_channels() const
{
	return num_channels_;
}

void Logic::push_segment(shared_ptr<LogicSegment> &segment)
{
	segments_.push_back(segment);

	if ((samplerate_ == 1) && (segment->samplerate() > 1))
		samplerate_ = segment->samplerate();
}

const deque< shared_ptr<LogicSegment> >& Logic::logic_segments() const
{
	return segments_;
}

deque< shared_ptr<LogicSegment> >& Logic::logic_segments()
{
	return segments_;
}

vector< shared_ptr<Segment> > Logic::segments() const
{
	return vector< shared_ptr<Segment> >(segments_.begin(), segments_.end());
}

uint32_t Logic::get_segment_count() const
{
	return (uint32_t)segments_.size();
}

void Logic::clear()
{
	segments_.clear();

	samples_cleared();
}

void Logic::set_samplerate(double value)
{
	samplerate_ = value;
}

double Logic::get_samplerate() const
{
	return samplerate_;
}

uint64_t Logic::max_sample_count() const
{
	uint64_t l = 0;
	for (const shared_ptr<LogicSegment>& s : segments_) {
		assert(s);
		l = max(l, s->get_sample_count());
	}
	return l;
}

void Logic::notify_samples_added(shared_ptr<Segment> segment, uint64_t start_sample,
	uint64_t end_sample)
{
	samples_added(segment, start_sample, end_sample);
}

} // namespace data
} // namespace pv
