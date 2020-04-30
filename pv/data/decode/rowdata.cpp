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

#include <cassert>

#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/data/decode/rowdata.hpp>

using std::vector;

namespace pv {
namespace data {
namespace decode {

RowData::RowData(Row* row) :
	row_(row),
	prev_ann_start_sample_(0)
{
	assert(row);
}

const Row* RowData::row() const
{
	return row_;
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
	deque<const pv::data::decode::Annotation*> &dest,
	uint64_t start_sample, uint64_t end_sample) const
{
	// Determine whether we must apply per-class filtering or not
	bool all_ann_classes_enabled = true;
	bool all_ann_classes_disabled = true;

	uint32_t max_ann_class_id = 0;
	for (AnnotationClass* c : row_->ann_classes()) {
		if (!c->visible())
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
				dest.push_back(&annotation);
	} else {
		if (!all_ann_classes_disabled) {
			// Filter out invisible annotation classes
			vector<size_t> class_visible;
			class_visible.resize(max_ann_class_id + 1, 0);
			for (AnnotationClass* c : row_->ann_classes())
				if (c->visible())
					class_visible[c->id] = 1;

			for (const auto& annotation : annotations_)
				if ((class_visible[annotation.ann_class_id()]) &&
					(annotation.end_sample() > start_sample) &&
					(annotation.start_sample() <= end_sample))
					dest.push_back(&annotation);
		}
	}
}

const deque<Annotation>& RowData::annotations() const
{
	return annotations_;
}

const Annotation* RowData::emplace_annotation(srd_proto_data *pdata)
{
	const srd_proto_data_annotation *const pda = (const srd_proto_data_annotation*)pdata->data;

	Annotation::Class ann_class_id = (Annotation::Class)(pda->ann_class);

	// Look up the longest annotation text to see if we have it in storage.
	// This implies that if the longest text is the same, the shorter texts
	// are expected to be the same, too. PDs that violate this assumption
	// should be considered broken.
	const char* const* ann_texts = (char**)pda->ann_text;
	const QString ann0 = QString::fromUtf8(ann_texts[0]);
	vector<QString>* storage_entry = &(ann_texts_[ann0]);

	if (storage_entry->empty()) {
		while (*ann_texts) {
			storage_entry->emplace_back(QString::fromUtf8(*ann_texts));
			ann_texts++;
		}
		storage_entry->shrink_to_fit();
	}


	const Annotation* result = nullptr;

	// We insert the annotation in a way so that the annotation list
	// is sorted by start sample. Otherwise, we'd have to sort when
	// painting, which is expensive

	if (pdata->start_sample < prev_ann_start_sample_) {
		// Find location to insert the annotation at

		auto it = annotations_.end();
		do {
			it--;
		} while ((it->start_sample() > pdata->start_sample) && (it != annotations_.begin()));

		// Allow inserting at the front
		if (it != annotations_.begin())
			it++;

		it = annotations_.emplace(it, pdata->start_sample, pdata->end_sample,
			storage_entry, ann_class_id, this);
		result = &(*it);
	} else {
		annotations_.emplace_back(pdata->start_sample, pdata->end_sample,
			storage_entry, ann_class_id, this);
		result = &(annotations_.back());
		prev_ann_start_sample_ = pdata->start_sample;
	}

	return result;
}

}  // namespace decode
}  // namespace data
}  // namespace pv
