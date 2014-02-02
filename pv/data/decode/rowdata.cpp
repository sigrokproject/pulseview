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

#include <boost/bind.hpp>

#include <assert.h>

#include "rowdata.h"

using std::vector;

namespace pv {
namespace data {
namespace decode {

RowData::RowData()
{
}

uint64_t RowData::get_max_sample() const
{
	if (_annotations.empty())
		return 0;
	return _annotations.back().end_sample();
}

void RowData::get_annotation_subset(
	std::vector<pv::data::decode::Annotation> &dest,
	uint64_t start_sample, uint64_t end_sample) const
{
	const vector<size_t>::const_iterator start_iter =
		lower_bound(_ann_end_index.begin(),
			_ann_end_index.end(), start_sample,
			bind(&RowData::index_entry_end_sample_lt,
				this, _1, _2));

	const vector<size_t>::const_iterator end_iter =
		upper_bound(_ann_start_index.begin(),
			_ann_start_index.end(), end_sample,
			bind(&RowData::index_entry_start_sample_gt,
				this, _1, _2));

	for (vector<size_t>::const_iterator i = start_iter;
		i != _ann_end_index.end() && *i != *end_iter; i++)
		dest.push_back(_annotations[*i]);
}

void RowData::push_annotation(const Annotation &a)
{
	const size_t offset = _annotations.size();
	_annotations.push_back(a);

	// Insert the annotation into the start index
	vector<size_t>::iterator i = _ann_start_index.end();
	if (!_ann_start_index.empty() &&
		_annotations[_ann_start_index.back()].start_sample() >
			a.start_sample())
		i = upper_bound(_ann_start_index.begin(),
			_ann_start_index.end(), a.start_sample(),
			bind(&RowData::index_entry_start_sample_gt,
				this, _1, _2));

	_ann_start_index.insert(i, offset);

	// Insert the annotation into the end index
	vector<size_t>::iterator j = _ann_end_index.end();
	if (!_ann_end_index.empty() &&
		_annotations[_ann_end_index.back()].end_sample() <
			a.end_sample())
		j = upper_bound(_ann_end_index.begin(),
			_ann_end_index.end(), a.end_sample(),
			bind(&RowData::index_entry_end_sample_gt,
				this, _1, _2));

	_ann_end_index.insert(j, offset);
}

bool RowData::index_entry_start_sample_gt(
	const uint64_t sample, const size_t index) const
{
	assert(index < _annotations.size());
	return _annotations[index].start_sample() > sample;
}

bool RowData::index_entry_end_sample_lt(
	const size_t index, const uint64_t sample) const
{
	assert(index < _annotations.size());
	return _annotations[index].end_sample() < sample;
}

bool RowData::index_entry_end_sample_gt(
	const uint64_t sample, const size_t index) const
{
	assert(index < _annotations.size());
	return _annotations[index].end_sample() > sample;
}

} // decode
} // data
} // pv
