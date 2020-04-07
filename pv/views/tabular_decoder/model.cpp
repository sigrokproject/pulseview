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

#include <QString>

#include "pv/views/tabular_decoder/view.hpp"

using std::make_shared;

namespace pv {
namespace views {
namespace tabular_decoder {

AnnotationCollectionModel::AnnotationCollectionModel(QObject* parent) :
	QAbstractItemModel(parent)
{
	vector<QVariant> header_data;
	header_data.emplace_back(tr("ID"));                // Column #0
	header_data.emplace_back(tr("Start Time"));        // Column #1
	header_data.emplace_back(tr("End Time"));          // Column #2
	header_data.emplace_back(tr("Ann Row Name"));      // Column #3
	header_data.emplace_back(tr("Class Row Name"));    // Column #4
	header_data.emplace_back(tr("Value"));             // Column #5
	root_ = make_shared<AnnotationCollectionItem>(header_data);
}

QVariant AnnotationCollectionModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole)
	{
		AnnotationCollectionItem* item =
			static_cast<AnnotationCollectionItem*>(index.internalPointer());

		return item->data(index.column());
	}

	if ((role == Qt::FontRole) && (index.parent().isValid()) && (index.column() == 0))
	{
		QFont font;
		font.setItalic(true);
		return QVariant(font);
	}

	return QVariant();
}

Qt::ItemFlags AnnotationCollectionModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant AnnotationCollectionModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return root_->data(section);

	return QVariant();
}

QModelIndex AnnotationCollectionModel::index(int row, int column,
	const QModelIndex& parent_idx) const
{
	if (!hasIndex(row, column, parent_idx))
		return QModelIndex();

	AnnotationCollectionItem* parent = root_.get();

	if (parent_idx.isValid())
		parent = static_cast<AnnotationCollectionItem*>(parent_idx.internalPointer());

	AnnotationCollectionItem* subItem = parent->subItem(row).get();

	return subItem ? createIndex(row, column, subItem) : QModelIndex();
}

QModelIndex AnnotationCollectionModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	AnnotationCollectionItem* subItem =
		static_cast<AnnotationCollectionItem*>(index.internalPointer());

	shared_ptr<AnnotationCollectionItem> parent = subItem->parent();

	return (parent == root_) ? QModelIndex() :
		createIndex(parent->row(), 0, parent.get());
}

int AnnotationCollectionModel::rowCount(const QModelIndex& parent_idx) const
{
	AnnotationCollectionItem* parent = root_.get();

	if (parent_idx.column() > 0)
		return 0;

	if (parent_idx.isValid())
		parent = static_cast<AnnotationCollectionItem*>(parent_idx.internalPointer());

	return parent->subItemCount();
}

int AnnotationCollectionModel::columnCount(const QModelIndex& parent_idx) const
{
	if (parent_idx.isValid())
		return static_cast<AnnotationCollectionItem*>(
			parent_idx.internalPointer())->columnCount();
	else
		return root_->columnCount();
}


} // namespace tabular_decoder
} // namespace views
} // namespace pv
