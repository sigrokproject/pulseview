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

using std::make_shared;

namespace pv {
namespace views {
namespace tabular_decoder {

AnnotationCollectionModel::AnnotationCollectionModel(QObject* parent) :
	QAbstractTableModel(parent),
	all_annotations_(nullptr),
	prev_segment_(0),
	prev_last_row_(0)
{
	// TBD Maybe use empty columns as indentation levels to indicate stacked decoders
	header_data_.emplace_back(tr("Start Sample"));      // Column #0
	header_data_.emplace_back(tr("Start Time"));        // Column #1
	header_data_.emplace_back(tr("Ann Row Name"));      // Column #2
	header_data_.emplace_back(tr("Ann Class Name"));    // Column #3
	header_data_.emplace_back(tr("Value"));             // Column #4
}

QVariant AnnotationCollectionModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole) {
		const Annotation* ann =
			static_cast<const Annotation*>(index.internalPointer());

		switch (index.column()) {
		case 0: return QVariant((qulonglong)ann->start_sample());  // Column #0, Start Sample
		case 1: return QVariant(0/*(qulonglong)ann->start_sample()*/);  // Column #1, Start Time
		case 2: return QVariant(ann->row()->title());              // Column #2, Ann Row Name
		case 3: return QVariant(ann->ann_class_name());            // Column #3, Ann Class Name
		case 4: return QVariant(ann->longest_annotation());        // Column #4, Value
		default: return QVariant();
		}
	}

	return QVariant();
}

Qt::ItemFlags AnnotationCollectionModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant AnnotationCollectionModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return header_data_.at(section);

	return QVariant();
}

QModelIndex AnnotationCollectionModel::index(int row, int column,
	const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	if (!all_annotations_)
		return QModelIndex();

	if ((size_t)row > all_annotations_->size())
		return QModelIndex();

	return createIndex(row, column, (void*)(all_annotations_->at(row)));
}

QModelIndex AnnotationCollectionModel::parent(const QModelIndex& index) const
{
	(void)index;

	return QModelIndex();
}

int AnnotationCollectionModel::rowCount(const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	if (!all_annotations_)
		return 0;

	return all_annotations_->size();
}

int AnnotationCollectionModel::columnCount(const QModelIndex& parent_idx) const
{
	(void)parent_idx;

	return header_data_.size();
}

void AnnotationCollectionModel::set_signal_and_segment(data::DecodeSignal* signal, uint32_t current_segment)
{
	all_annotations_ = signal->get_all_annotations_by_segment(current_segment);

	if (!all_annotations_ || all_annotations_->empty()) {
		prev_segment_ = current_segment;
		return;
	}

	const size_t new_row_count = all_annotations_->size() - 1;

	// Force the view associated with this model to update when the segment changes
	if (prev_segment_ != current_segment) {
		dataChanged(QModelIndex(), QModelIndex());
		layoutChanged();
	} else {
		// Force the view associated with this model to update when we have more annotations
		if (prev_last_row_ < new_row_count) {
			dataChanged(index(prev_last_row_, 0, QModelIndex()),
				index(new_row_count, 0, QModelIndex()));
			layoutChanged();
		}
	}

	prev_segment_ = current_segment;
	prev_last_row_ = new_row_count;
}

} // namespace tabular_decoder
} // namespace views
} // namespace pv
