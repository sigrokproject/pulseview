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

#include "subwindow.hpp"

using std::out_of_range;

namespace pv {
namespace subwindows {
namespace decoder_selector {

DecoderCollectionItem::DecoderCollectionItem(const vector<QVariant>& data,
	shared_ptr<DecoderCollectionItem> parent) :
	data_(data),
	parent_(parent)
{
}

void DecoderCollectionItem::appendSubItem(shared_ptr<DecoderCollectionItem> item)
{
	subItems_.push_back(item);
}

shared_ptr<DecoderCollectionItem> DecoderCollectionItem::subItem(int row) const
{
	try {
		return subItems_.at(row);
	} catch (out_of_range) {
		return nullptr;
	}
}

shared_ptr<DecoderCollectionItem> DecoderCollectionItem::parent() const
{
	return parent_;
}

shared_ptr<DecoderCollectionItem> DecoderCollectionItem::findSubItem(
	const QVariant& value, int column)
{
	for (shared_ptr<DecoderCollectionItem> item : subItems_)
		if (item->data(column) == value)
			return item;

	return nullptr;
}

int DecoderCollectionItem::subItemCount() const
{
	return subItems_.size();
}

int DecoderCollectionItem::columnCount() const
{
	return data_.size();
}

int DecoderCollectionItem::row() const
{
	if (parent_)
		for (uint i = 0; i < parent_->subItems_.size(); i++)
			if (parent_->subItems_.at(i).get() == const_cast<DecoderCollectionItem*>(this))
				return i;

	return 0;
}

QVariant DecoderCollectionItem::data(int column) const
{
	try {
		return data_.at(column);
	} catch (out_of_range) {
		return QVariant();
	}
}

} // namespace decoder_selector
} // namespace subwindows
} // namespace pv
