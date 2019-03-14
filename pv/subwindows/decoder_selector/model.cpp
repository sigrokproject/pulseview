/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2018 Soeren Apel <soeren@apelpie.net>
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

#include "subwindow.hpp"

#include <libsigrokdecode/libsigrokdecode.h>

using std::make_shared;

namespace pv {
namespace subwindows {
namespace decoder_selector {

DecoderCollectionModel::DecoderCollectionModel(QObject* parent) :
	QAbstractItemModel(parent)
{
	vector<QVariant> header_data;
	header_data.emplace_back(tr("Decoder"));     // Column #0
	header_data.emplace_back(tr("Name"));        // Column #1
	header_data.emplace_back(tr("ID"));          // Column #2
	root_ = make_shared<DecoderCollectionItem>(header_data);

	// Note: the tag groups are sub-items of the root item

	// Create "all decoders" group
	vector<QVariant> item_data;
	item_data.emplace_back(tr("All Decoders"));
	// Add dummy entries to make the row count the same as the
	// sub-item size, or else we can't query sub-item data
	item_data.emplace_back();
	item_data.emplace_back();
	shared_ptr<DecoderCollectionItem> group_item_all =
		make_shared<DecoderCollectionItem>(item_data, root_);
	root_->appendSubItem(group_item_all);

	for (GSList* li = (GSList*)srd_decoder_list(); li; li = li->next) {
		const srd_decoder *const d = (srd_decoder*)li->data;
		assert(d);

		const QString id = QString::fromUtf8(d->id);
		const QString name = QString::fromUtf8(d->name);
		const QString long_name = QString::fromUtf8(d->longname);

		// Add decoder to the "all decoders" group
		item_data.clear();
		item_data.emplace_back(name);
		item_data.emplace_back(long_name);
		item_data.emplace_back(id);
		shared_ptr<DecoderCollectionItem> decoder_item_all =
			make_shared<DecoderCollectionItem>(item_data, group_item_all);
		group_item_all->appendSubItem(decoder_item_all);

		// Add decoder to all relevant groups using the tag information
		for (GSList* ti = (GSList*)d->tags; ti; ti = ti->next) {
			const QString tag = tr((char*)ti->data);
			const QVariant tag_var = QVariant(tag);

			// Find tag group and create it if it doesn't exist yet
			shared_ptr<DecoderCollectionItem> group_item =
				root_->findSubItem(tag_var, 0);

			if (!group_item) {
				item_data.clear();
				item_data.emplace_back(tag);
				// Add dummy entries to make the row count the same as the
				// sub-item size, or else we can't query sub-item data
				item_data.emplace_back();
				item_data.emplace_back();
				group_item = make_shared<DecoderCollectionItem>(item_data, root_);
				root_->appendSubItem(group_item);
			}

			// Create decoder item
			item_data.clear();
			item_data.emplace_back(name);
			item_data.emplace_back(long_name);
			item_data.emplace_back(id);
			shared_ptr<DecoderCollectionItem> decoder_item =
				make_shared<DecoderCollectionItem>(item_data, group_item);

			// Add decoder to tag group
			group_item->appendSubItem(decoder_item);
		}
	}
}

QVariant DecoderCollectionModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	DecoderCollectionItem* item =
		static_cast<DecoderCollectionItem*>(index.internalPointer());

	return item->data(index.column());
}

Qt::ItemFlags DecoderCollectionModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant DecoderCollectionModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return root_->data(section);

	return QVariant();
}

QModelIndex DecoderCollectionModel::index(int row, int column,
	const QModelIndex& parent_idx) const
{
	if (!hasIndex(row, column, parent_idx))
		return QModelIndex();

	DecoderCollectionItem* parent = root_.get();

	if (parent_idx.isValid())
		parent = static_cast<DecoderCollectionItem*>(parent_idx.internalPointer());

	DecoderCollectionItem* subItem = parent->subItem(row).get();

	return subItem ? createIndex(row, column, subItem) : QModelIndex();
}

QModelIndex DecoderCollectionModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	DecoderCollectionItem* subItem =
		static_cast<DecoderCollectionItem*>(index.internalPointer());

	shared_ptr<DecoderCollectionItem> parent = subItem->parent();

	return (parent == root_) ? QModelIndex() :
		createIndex(parent->row(), 0, parent.get());
}

int DecoderCollectionModel::rowCount(const QModelIndex& parent_idx) const
{
	DecoderCollectionItem* parent = root_.get();

	if (parent_idx.column() > 0)
		return 0;

	if (parent_idx.isValid())
		parent = static_cast<DecoderCollectionItem*>(parent_idx.internalPointer());

	return parent->subItemCount();
}

int DecoderCollectionModel::columnCount(const QModelIndex& parent_idx) const
{
	if (parent_idx.isValid())
		return static_cast<DecoderCollectionItem*>(
			parent_idx.internalPointer())->columnCount();
	else
		return root_->columnCount();
}


} // namespace decoder_selector
} // namespace subwindows
} // namespace pv
