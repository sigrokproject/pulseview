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

#include "pv/views/tabular_decoder/view.hpp"

using std::out_of_range;

namespace pv {
namespace views {
namespace tabular_decoder {

AnnotationCollectionItem::AnnotationCollectionItem(const vector<QVariant>& data,
	shared_ptr<AnnotationCollectionItem> parent) :
	data_(data),
	parent_(parent)
{
}

void AnnotationCollectionItem::appendSubItem(shared_ptr<AnnotationCollectionItem> item)
{
	subItems_.push_back(item);
}

shared_ptr<AnnotationCollectionItem> AnnotationCollectionItem::subItem(int row) const
{
	try {
		return subItems_.at(row);
	} catch (out_of_range&) {
		return nullptr;
	}
}

shared_ptr<AnnotationCollectionItem> AnnotationCollectionItem::parent() const
{
	return parent_;
}

shared_ptr<AnnotationCollectionItem> AnnotationCollectionItem::findSubItem(
	const QVariant& value, int column)
{
	for (shared_ptr<AnnotationCollectionItem> item : subItems_)
		if (item->data(column) == value)
			return item;

	return nullptr;
}

int AnnotationCollectionItem::subItemCount() const
{
	return subItems_.size();
}

int AnnotationCollectionItem::columnCount() const
{
	return data_.size();
}

int AnnotationCollectionItem::row() const
{
	if (parent_)
		for (size_t i = 0; i < parent_->subItems_.size(); i++)
			if (parent_->subItems_.at(i).get() == const_cast<AnnotationCollectionItem*>(this))
				return i;

	return 0;
}

QVariant AnnotationCollectionItem::data(int column) const
{
	try {
		return data_.at(column);
	} catch (out_of_range&) {
		return QVariant();
	}
}

} // namespace tabular_decoder
} // namespace views
} // namespace pv
