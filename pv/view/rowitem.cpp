/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <assert.h>

#include "view.h"

#include "rowitem.h"

namespace pv {
namespace view {

RowItem::RowItem() :
	_owner(NULL),
	_v_offset(0)
{
}

int RowItem::v_offset() const
{
	return _v_offset;
}

void RowItem::set_v_offset(int v_offset)
{
	_v_offset = v_offset;
}

RowItemOwner* RowItem::owner() const
{
	return _owner;
}

void RowItem::set_owner(RowItemOwner *owner)
{
	assert((_owner && !owner) || (!_owner && owner));
	_owner = owner;
}

int RowItem::get_y() const
{
	assert(_owner);
	return _v_offset + _owner->owner_v_offset();
}

QPoint RowItem::point() const
{
	return QPoint(0, v_offset());
}

void RowItem::paint_back(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void RowItem::paint_mid(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void RowItem::paint_fore(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void RowItem::hover_point_changed()
{
}

} // namespace view
} // namespace pv
