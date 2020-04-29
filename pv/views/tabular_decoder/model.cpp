/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
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

#include <QDebug>
#include <QString>

#include "pv/views/tabular_decoder/view.hpp"

#include "view.hpp"

#include "pv/util.hpp"

using std::make_shared;

using pv::util::Timestamp;
using pv::util::format_time_si;
using pv::util::format_time_minutes;
using pv::util::SIPrefix;

namespace pv {
namespace views {
namespace tabular_decoder {

AnnotationCollectionModel::AnnotationCollectionModel(QObject* parent) :
	QAbstractTableModel(parent),
	all_annotations_(nullptr),
	dataset_(nullptr),
	signal_(nullptr),
	prev_segment_(0),
	prev_last_row_(0),
	start_index_(0),
	end_index_(0),
	hide_hidden_(false)
{
	GlobalSettings::add_change_handler(this);
	theme_is_dark_ = GlobalSettings::current_theme_is_dark();

	// TBD Maybe use empty columns as indentation levels to indicate stacked decoders
	header_data_.emplace_back(tr("Sample"));     // Column #0
	header_data_.emplace_back(tr("Time"));       // Column #1
	header_data_.emplace_back(tr("Decoder"));    // Column #2
	header_data_.emplace_back(tr("Ann Row"));    // Column #3
	header_data_.emplace_back(tr("Ann Class"));  // Column #4
	header_data_.emplace_back(tr("Value"));      // Column #5
}

QVariant AnnotationCollectionModel::data_from_ann(const Annotation* ann, int index) const
{
	switch (index) {
	case 0: return QVariant((qulonglong)ann->start_sample());  // Column #0, Start Sample
	case 1: {                                                  // Column #1, Start Time
			Timestamp t = ann->start_sample() / signal_->get_samplerate();
			QString unit = signal_->get_samplerate() ? tr("s") : tr("sa");
			QString s;
			if ((t < 60) || (signal_->get_samplerate() == 0))  // i.e. if unit is sa
				s = format_time_si(t, SIPrefix::unspecified, 3, unit, false);
			else
				s = format_time_minutes(t, 3, false);
			return QVariant(s);
		}
	case 2: return QVariant(ann->row()->decoder()->name());    // Column #2, Decoder
	case 3: return QVariant(ann->row()->description());        // Column #3, Ann Row
	case 4: return QVariant(ann->ann_class_description());     // Column #4, Ann Class
	case 5: return QVariant(ann->longest_annotation());        // Column #5, Value
	default: return QVariant();
	}
}

QVariant AnnotationCollectionModel::data(const QModelIndex& index, int role) const
{
	if (!signal_ || !index.isValid() || !index.internalPointer())
		return QVariant();

	const Annotation* ann =
		static_cast<const Annotation*>(index.internalPointer());

	if ((role == Qt::DisplayRole) || (role == Qt::ToolTipRole))
		return data_from_ann(ann, index.column());

	if (role == Qt::BackgroundRole) {
		int level = 0;

		const unsigned int ann_stack_level = ann->row_data()->row()->decoder()->get_stack_level();
		level = (signal_->decoder_stack().size() - 1 - ann_stack_level);

		// Only use custom cell background color if column index reached the hierarchy level
		if (index.column() >= level) {
			if (theme_is_dark_)
				return QBrush(ann->dark_color());
			else
				return QBrush(ann->bright_color());
		}
	}

	return QVariant();
}

Qt::ItemFlags AnnotationCollectionModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

QVariant AnnotationCollectionModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
		return header_data_.at(section);

	return QVariant();
}

QModelIndex AnnotationCollectionModel::index(int row, int column,
	const QModelIndex& parent_idx) const
{
	(void)parent_idx;
	assert(column >= 0);

	if (!dataset_ || (row < 0))
		return QModelIndex();

	QModelIndex idx;

	if (start_index_ == end_index_) {
		if ((size_t)row < dataset_->size())
			idx = createIndex(row, column, (void*)dataset_->at(row));
	} else {
		if ((size_t)row < (end_index_ - start_index_))
			idx = createIndex(row, column, (void*)dataset_->at(start_index_ + row));
	}

	return idx;
}

QModelIndex AnnotationCollectionModel::parent(const QModelIndex& index) const
{
	(void)index;

	return QModelIndex();
}

int AnnotationCollectionModel::rowCount(const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	if (!dataset_)
		return 0;

	if (start_index_ == end_index_)
		return dataset_->size();
	else
		return (end_index_ - start_index_);
}

int AnnotationCollectionModel::columnCount(const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	return header_data_.size();
}

void AnnotationCollectionModel::set_signal_and_segment(data::DecodeSignal* signal, uint32_t current_segment)
{
	if (!signal) {
		all_annotations_ = nullptr;
		dataset_ = nullptr;
		signal_ = nullptr;

		dataChanged(QModelIndex(), QModelIndex());
		layoutChanged();
		return;
	}

	all_annotations_ = signal->get_all_annotations_by_segment(current_segment);
	signal_ = signal;

	if (hide_hidden_)
		update_annotations_without_hidden();
	else
		dataset_ = all_annotations_;

	if (!dataset_ || dataset_->empty()) {
		prev_segment_ = current_segment;
		return;
	}

	// Re-apply the requested sample range
	set_sample_range(start_sample_, end_sample_);

	const size_t new_row_count = dataset_->size() - 1;

	// Force the view associated with this model to update when the segment changes
	if (prev_segment_ != current_segment) {
		dataChanged(QModelIndex(), QModelIndex());
		layoutChanged();
	} else {
		// Force the view associated with this model to update when we have more annotations
		if (prev_last_row_ < new_row_count) {
			dataChanged(index(prev_last_row_, 0), index(new_row_count, 0));
			layoutChanged();
		}
	}

	prev_segment_ = current_segment;
	prev_last_row_ = new_row_count;
}

void AnnotationCollectionModel::set_sample_range(uint64_t start_sample, uint64_t end_sample)
{
	// Check if there's even anything to reset
	if ((start_sample == end_sample) && (start_index_ == end_index_))
		return;

	if (!dataset_ || dataset_->empty() || (end_sample == 0)) {
		start_index_ = 0;
		end_index_ = 0;
		start_sample_ = 0;
		end_sample_ = 0;

		dataChanged(QModelIndex(), QModelIndex());
		layoutChanged();
		return;
	}

	start_sample_ = start_sample;
	end_sample_ = end_sample;

	// Determine first and last indices into the annotation list
	int64_t i = -1;
	bool ann_outside_range;
	do {
		i++;

		if (i == (int64_t)dataset_->size()) {
			start_index_ = 0;
			end_index_ = 0;

			dataChanged(QModelIndex(), QModelIndex());
			layoutChanged();
			return;
		}
		const Annotation* ann = (*dataset_)[i];
		ann_outside_range =
			((ann->start_sample() < start_sample) && (ann->end_sample() < start_sample));
	} while (ann_outside_range);
	start_index_ = i;

	// Ideally, we would be able to set end_index_ to the last annotation that
	// is within range. However, as annotations in the list are sorted by
	// start sample and hierarchy level, we may encounter this scenario:
	//   [long annotation that spans across view]
	//   [short annotations that aren't seen]
	//   [short annotations that are seen]
	// ..in which our output would only show the first long annotations.
	// For this reason, we simply show everything after the first visible
	// annotation for now.

	end_index_ = dataset_->size();

	dataChanged(index(0, 0), index((end_index_ - start_index_), 0));
	layoutChanged();
}

void AnnotationCollectionModel::set_hide_hidden(bool hide_hidden)
{
	hide_hidden_ = hide_hidden;

	if (hide_hidden_) {
		dataset_ = &all_annotations_without_hidden_;
		update_annotations_without_hidden();
	} else {
		dataset_ = all_annotations_;
		all_annotations_without_hidden_.clear();  // To conserve memory
	}

	// Re-apply the requested sample range
	set_sample_range(start_sample_, end_sample_);

	if (dataset_)
		dataChanged(index(0, 0), index(dataset_->size(), 0));
	else
		dataChanged(QModelIndex(), QModelIndex());

	layoutChanged();
}

void AnnotationCollectionModel::update_annotations_without_hidden()
{
	uint64_t count = 0;

	if (!all_annotations_ || all_annotations_->empty()) {
		all_annotations_without_hidden_.clear();
		return;
	}

	for (const Annotation* ann : *all_annotations_) {
		if (!ann->visible())
			continue;

		if (all_annotations_without_hidden_.size() < (count + 100))
			all_annotations_without_hidden_.resize(count + 100);

		all_annotations_without_hidden_[count++] = ann;
	}

	all_annotations_without_hidden_.resize(count);
}

void AnnotationCollectionModel::on_setting_changed(const QString &key, const QVariant &value)
{
	(void)key;
	(void)value;

	// We don't really care about the actual setting, we just update the
	// flag that indicates whether we are using a bright or dark color theme
	theme_is_dark_ = GlobalSettings::current_theme_is_dark();
}

} // namespace tabular_decoder
} // namespace views
} // namespace pv
