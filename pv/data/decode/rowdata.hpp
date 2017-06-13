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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_DATA_DECODE_ROWDATA_HPP
#define PULSEVIEW_PV_DATA_DECODE_ROWDATA_HPP

#include <vector>

#include <libsigrokdecode/libsigrokdecode.h>

#include "annotation.hpp"

using std::vector;

namespace pv {
namespace data {
namespace decode {

class Row;

class RowData
{
public:
	RowData() = default;

public:
	uint64_t get_max_sample() const;

	/**
	 * Extracts annotations between the given sample range into a vector.
	 * Note: The annotations are unsorted and only annotations that fully
	 * fit into the sample range are considered.
	 */
	void get_annotation_subset(
		vector<pv::data::decode::Annotation> &dest,
		uint64_t start_sample, uint64_t end_sample) const;

	void emplace_annotation(srd_proto_data *pdata, const Row *row);

private:
	vector<Annotation> annotations_;
};

}  // namespace decode
}  // namespace data
}  // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODE_ROWDATA_HPP
