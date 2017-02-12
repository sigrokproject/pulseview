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

#ifndef PULSEVIEW_PV_DATA_ANALOGSEGMENT_HPP
#define PULSEVIEW_PV_DATA_ANALOGSEGMENT_HPP

#include "segment.hpp"

#include <utility>
#include <vector>

namespace AnalogSegmentTest {
struct Basic;
}

namespace pv {
namespace data {

typedef struct {
	uint64_t sample_index, chunk_num, chunk_offs;
	uint8_t* chunk;
	float* value;
} SegmentAnalogDataIterator;

class AnalogSegment : public Segment
{
public:
	struct EnvelopeSample
	{
		float min;
		float max;
	};

	struct EnvelopeSection
	{
		uint64_t start;
		unsigned int scale;
		uint64_t length;
		EnvelopeSample *samples;
	};

private:
	struct Envelope
	{
		uint64_t length;
		uint64_t data_length;
		EnvelopeSample *samples;
	};

private:
	static const unsigned int ScaleStepCount = 10;
	static const int EnvelopeScalePower;
	static const int EnvelopeScaleFactor;
	static const float LogEnvelopeScaleFactor;
	static const uint64_t EnvelopeDataUnit;

public:
	AnalogSegment(uint64_t samplerate);

	virtual ~AnalogSegment();

	void append_interleaved_samples(const float *data,
		size_t sample_count, size_t stride);

	const float* get_samples(int64_t start_sample,
		int64_t end_sample) const;

	const std::pair<float, float> get_min_max() const;

	SegmentAnalogDataIterator* begin_sample_iteration(uint64_t start) const;
	void continue_sample_iteration(SegmentAnalogDataIterator* it, uint64_t increase) const;
	void end_sample_iteration(SegmentAnalogDataIterator* it) const;

	void get_envelope_section(EnvelopeSection &s,
		uint64_t start, uint64_t end, float min_length) const;

private:
	void reallocate_envelope(Envelope &e);

	void append_payload_to_envelope_levels();

private:
	struct Envelope envelope_levels_[ScaleStepCount];

	float min_value_, max_value_;

	friend struct AnalogSegmentTest::Basic;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_ANALOGSEGMENT_HPP
