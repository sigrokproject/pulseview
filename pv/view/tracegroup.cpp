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

#include <algorithm>

#include "tracegroup.h"

using std::pair;
using std::shared_ptr;

namespace pv {
namespace view {

const int TraceGroup::Padding = 8;
const int TraceGroup::Width = 12;

TraceGroup::~TraceGroup()
{
	_owner = nullptr;
	clear_child_items();
}

bool TraceGroup::enabled() const
{
	return std::any_of(child_items().begin(), child_items().end(),
		[](const shared_ptr<RowItem> &r) { return r->enabled(); });
}

pv::SigSession& TraceGroup::session()
{
	assert(_owner);
	return _owner->session();
}

const pv::SigSession& TraceGroup::session() const
{
	assert(_owner);
	return _owner->session();
}

pv::view::View* TraceGroup::view()
{
	assert(_owner);
	return _owner->view();
}

const pv::view::View* TraceGroup::view() const
{
	assert(_owner);
	return _owner->view();
}

pair<int, int> TraceGroup::v_extents() const
{
	return RowItemOwner::v_extents();
}

void TraceGroup::paint_label(QPainter &p, int right, bool hover)
{
	(void)p;
	(void)right;
	(void)hover;
}

QRectF TraceGroup::label_rect(int right) const
{
	QRectF rect;
	for (const shared_ptr<RowItem> r : child_items())
		if (r)
			rect = rect.united(r->label_rect(right));

	return QRectF(rect.x() - Width - Padding, rect.y(),
		Width, rect.height());
}

bool TraceGroup::pt_in_label_rect(int left, int right, const QPoint &point)
{
	(void)left;
	(void)right;
	(void)point;

	return false;
}

QMenu* TraceGroup::create_context_menu(QWidget *parent)
{
	(void)parent;

	return NULL;
}

pv::widgets::Popup* TraceGroup::create_popup(QWidget *parent)
{
	(void)parent;
	return NULL;
}

int TraceGroup::owner_v_offset() const
{
	return v_offset() + _owner->owner_v_offset();
}

void TraceGroup::update_viewport()
{
	if (_owner)
		_owner->update_viewport();
}

} // namespace view
} // namespace pv
