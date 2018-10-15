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

#include <mutex>
#include <thread>
#include <vector>

#include <QObject>

using std::recursive_mutex;
using std::vector;

namespace SegmentTest {
struct SmallSize8Single;
struct MediumSize8Single;
struct MaxSize8Single;
struct MediumSize24Single;
struct MediumSize32Single;
struct MaxSize32Single;
struct MediumSize32Multi;
struct MaxSize32Multi;
struct MaxSize32MultiAtOnce;
struct MaxSize32MultiIterated;
}  // namespace SegmentTest

namespace pv {
namespace data {

typedef struct {
	uint64_t sample_index, chunk_num, chunk_offs;
	uint8_t* chunk;
} SegmentDataIterator;

class Segment : public QObject
{
	Q_OBJECT

private:
	static const uint64_t MaxChunkSize;

public:
	Segment(uint32_t segment_id, uint64_t samplerate, unsigned int unit_size);

	virtual ~Segment();

	uint64_t get_sample_count() const;

	const pv::util::Timestamp& start_time() const;

	double samplerate() const;
	void set_samplerate(double samplerate);

	unsigned int unit_size() const;

	uint32_t segment_id() const;

	void set_complete();
	bool is_complete() const;

	void free_unused_memory();

protected:
	void append_single_sample(void *data);
	void append_samples(void *data, uint64_t samples);
	void get_raw_samples(uint64_t start, uint64_t count, uint8_t *dest) const;

	SegmentDataIterator* begin_sample_iteration(uint64_t start);
	void continue_sample_iteration(SegmentDataIterator* it, uint64_t increase);
	void end_sample_iteration(SegmentDataIterator* it);
	uint8_t* get_iterator_value(SegmentDataIterator* it);
	uint64_t get_iterator_valid_length(SegmentDataIterator* it);

	uint32_t segment_id_;
	mutable recursive_mutex mutex_;
	vector<uint8_t*> data_chunks_;
	uint8_t* current_chunk_;
	uint64_t used_samples_, unused_samples_;
	uint64_t sample_count_;
	pv::util::Timestamp start_time_;
	double samplerate_;
	uint64_t chunk_size_;
	unsigned int unit_size_;
	int iterator_count_;
	bool mem_optimization_requested_;
	bool is_complete_;

	friend struct SegmentTest::SmallSize8Single;
	friend struct SegmentTest::MediumSize8Single;
	friend struct SegmentTest::MaxSize8Single;
	friend struct SegmentTest::MediumSize24Single;
	friend struct SegmentTest::MediumSize32Single;
	friend struct SegmentTest::MaxSize32Single;
	friend struct SegmentTest::MediumSize32Multi;
	friend struct SegmentTest::MaxSize32Multi;
	friend struct SegmentTest::MaxSize32MultiAtOnce;
	friend struct SegmentTest::MaxSize32MultiIterated;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_SEGMENT_HPP
