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

#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/data/decode/rowdata.hpp>

using std::vector;

namespace pv {
namespace data {
namespace decode {

RowData::RowData(Row* row) :
	row_(row)
{
	assert(row);
}

uint64_t RowData::get_max_sample() const
{
	if (annotations_.empty())
		return 0;
	return annotations_.back().end_sample();
}

uint64_t RowData::get_annotation_count() const
{
	return annotations_.size();
}

void RowData::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	uint64_t start_sample, uint64_t end_sample) const
{
	// Determine whether we must apply per-class filtering or not
	bool all_ann_classes_enabled = true;
	bool all_ann_classes_disabled = true;

	uint32_t max_ann_class_id = 0;
	for (AnnotationClass* c : row_->ann_classes()) {
		if (!c->visible)
			all_ann_classes_enabled = false;
		else
			all_ann_classes_disabled = false;
		if (c->id > max_ann_class_id)
			max_ann_class_id = c->id;
	}

	if (all_ann_classes_enabled) {
		// No filtering, send everyting out as-is
		for (const auto& annotation : annotations_)
			if ((annotation.end_sample() > start_sample) &&
				(annotation.start_sample() <= end_sample))
				dest.push_back(annotation);
	} else {
		if (!all_ann_classes_disabled) {
			// Filter out invisible annotation classes
			vector<size_t> class_visible;
			class_visible.resize(max_ann_class_id + 1, 0);
			for (AnnotationClass* c : row_->ann_classes())
				if (c->visible)
					class_visible[c->id] = 1;

			for (const auto& annotation : annotations_)
				if ((class_visible[annotation.ann_class()]) &&
					(annotation.end_sample() > start_sample) &&
					(annotation.start_sample() <= end_sample))
					dest.push_back(annotation);
		}
	}
}

void RowData::emplace_annotation(srd_proto_data *pdata)
{
	annotations_.emplace_back(pdata, row_);
}

}  // namespace decode
}  // namespace data
}  // namespace pv
