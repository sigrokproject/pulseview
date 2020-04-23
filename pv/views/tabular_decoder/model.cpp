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

using std::make_shared;

namespace pv {
namespace views {
namespace tabular_decoder {

AnnotationCollectionModel::AnnotationCollectionModel(QObject* parent) :
	QAbstractTableModel(parent),
	all_annotations_(nullptr),
	signal_(nullptr),
	prev_segment_(0),
	prev_last_row_(0)
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

QVariant AnnotationCollectionModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || !signal_)
		return QVariant();

	const Annotation* ann =
		static_cast<const Annotation*>(index.internalPointer());

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0: return QVariant((qulonglong)ann->start_sample());  // Column #0, Start Sample
		case 1: return QVariant(0/*(qulonglong)ann->start_sample()*/);  // Column #1, Start Time
		case 2: return QVariant(ann->row()->decoder()->name());    // Column #2, Decoder
		case 3: return QVariant(ann->row()->description());        // Column #3, Ann Row
		case 4: return QVariant(ann->ann_class_description());     // Column #4, Ann Class
		case 5: return QVariant(ann->longest_annotation());        // Column #5, Value
		default: return QVariant();
		}
	}

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
	if (!signal) {
		all_annotations_ = nullptr;
		signal_ = nullptr;
		dataChanged(QModelIndex(), QModelIndex());
		layoutChanged();
		return;
	}

	all_annotations_ = signal->get_all_annotations_by_segment(current_segment);
	signal_ = signal;

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
