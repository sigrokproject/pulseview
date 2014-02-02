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

#ifndef PULSEVIEW_PV_DATA_DECODE_ROWDATA_H
#define PULSEVIEW_PV_DATA_DECODE_ROWDATA_H

#include <vector>

#include "annotation.h"

namespace pv {
namespace data {
namespace decode {

class RowData
{
public:
	RowData();

public:
	uint64_t get_max_sample() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation> &dest,
		uint64_t start_sample, uint64_t end_sample) const;

	void push_annotation(const Annotation& a);

private:
	bool index_entry_start_sample_gt(
		const uint64_t sample, const size_t index) const;
	bool index_entry_end_sample_lt(
		const size_t index, const uint64_t sample) const;
	bool index_entry_end_sample_gt(
		const uint64_t sample, const size_t index) const;

private:
	std::vector<Annotation> _annotations;

	/**
	 * _ann_start_index and _ann_end_index contain lists of annotions
	 * (represented by offsets in the _annotations vector), sorted in
	 * ascending ordered by the start_sample and end_sample respectively.
	 */
	std::vector<size_t> _ann_start_index, _ann_end_index;
};

}
} // data
} // pv

#endif // PULSEVIEW_PV_DATA_DECODE_ROWDATA_H
