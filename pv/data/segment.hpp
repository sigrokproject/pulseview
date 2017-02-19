/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_DATA_SEGMENT_HPP
#define PULSEVIEW_PV_DATA_SEGMENT_HPP

#include "pv/util.hpp"

#include <thread>
#include <mutex>
#include <vector>

namespace SegmentTest {
struct SmallSize8Single;
struct MediumSize8Single;
struct MaxSize8Single;
struct MediumSize24Single;
struct MediumSize32Single;
struct MaxSize32Single;
struct MediumSize32Multi;
struct MaxSize32Multi;
struct MaxSize32MultiIterated;
}

namespace pv {
namespace data {

typedef struct {
	uint64_t sample_index, chunk_num, chunk_offs;
	uint8_t* chunk;
	uint8_t* value;
} SegmentRawDataIterator;

class Segment
{
private:
	static const uint64_t MaxChunkSize;

public:
	Segment(uint64_t samplerate, unsigned int unit_size);

	virtual ~Segment();

	uint64_t get_sample_count() const;

	const pv::util::Timestamp& start_time() const;

	double samplerate() const;
	void set_samplerate(double samplerate);

	unsigned int unit_size() const;

	void free_unused_memory();

protected:
	void append_single_sample(void *data);
	void append_samples(void *data, uint64_t samples);
	uint8_t* get_raw_samples(uint64_t start, uint64_t count) const;

	SegmentRawDataIterator* begin_raw_sample_iteration(uint64_t start);
	void continue_raw_sample_iteration(SegmentRawDataIterator* it, uint64_t increase);
	void end_raw_sample_iteration(SegmentRawDataIterator* it);

	mutable std::recursive_mutex mutex_;
	std::vector<uint8_t*> data_chunks_;
	uint8_t* current_chunk_;
	uint64_t used_samples_, unused_samples_;
	uint64_t sample_count_;
	pv::util::Timestamp start_time_;
	double samplerate_;
	uint64_t chunk_size_;
	unsigned int unit_size_;
	int iterator_count_;
	bool mem_optimization_requested_;

	friend struct SegmentTest::SmallSize8Single;
	friend struct SegmentTest::MediumSize8Single;
	friend struct SegmentTest::MaxSize8Single;
	friend struct SegmentTest::MediumSize24Single;
	friend struct SegmentTest::MediumSize32Single;
	friend struct SegmentTest::MaxSize32Single;
	friend struct SegmentTest::MediumSize32Multi;
	friend struct SegmentTest::MaxSize32Multi;
	friend struct SegmentTest::MaxSize32MultiIterated;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_SEGMENT_HPP
