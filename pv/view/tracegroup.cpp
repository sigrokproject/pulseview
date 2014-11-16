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

#include <extdef.h>
#include <assert.h>

#include <algorithm>

#include <QMenu>
#include <QPainter>

#include "tracegroup.h"

using std::pair;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

const int TraceGroup::Padding = 8;
const int TraceGroup::Width = 12;
const int TraceGroup::LineThickness = 5;
const QColor TraceGroup::LineColour(QColor(0x55, 0x57, 0x53));

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
	const QRectF r = label_rect(right).adjusted(
		LineThickness / 2, LineThickness / 2,
		-LineThickness / 2, -LineThickness / 2);

	// Paint the label
	const QPointF points[] = {
		r.topRight(),
		r.topLeft(),
		r.bottomLeft(),
		r.bottomRight()
	};

	if (selected()) {
		const QPen pen(highlight_pen());
		p.setPen(QPen(pen.brush(), pen.width() + LineThickness,
			Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
		p.setBrush(Qt::transparent);
		p.drawPolyline(points, countof(points));
	}

	p.setPen(QPen(QBrush(LineColour.darker()), LineThickness,
		Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
	p.drawPolyline(points, countof(points));
	p.setPen(QPen(QBrush(hover ? LineColour.lighter() : LineColour),
		LineThickness - 2, Qt::SolidLine, Qt::SquareCap,
		Qt::RoundJoin));
	p.drawPolyline(points, countof(points));
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
	QMenu *const menu = new QMenu(parent);

	QAction *const ungroup = new QAction(tr("Ungroup"), this);
	ungroup->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_U));
	connect(ungroup, SIGNAL(triggered()), this, SLOT(on_ungroup()));
	menu->addAction(ungroup);

	return menu;
}

pv::widgets::Popup* TraceGroup::create_popup(QWidget *parent)
{
	(void)parent;
	return NULL;
}

int TraceGroup::owner_v_offset() const
{
	return _owner ? layout_v_offset() + _owner->owner_v_offset() : 0;
}

unsigned int TraceGroup::depth() const
{
	return _owner ? _owner->depth() + 1 : 0;
}

void TraceGroup::on_ungroup()
{
	const vector< shared_ptr<RowItem> > items(
		child_items().begin(), child_items().end());
	clear_child_items();

	for (shared_ptr<RowItem> r : items)
		_owner->add_child_item(r);

	_owner->remove_child_item(shared_from_this());
}

void TraceGroup::appearance_changed(bool label, bool content)
{
	if (_owner)
		_owner->appearance_changed(label, content);
}

void TraceGroup::extents_changed(bool horz, bool vert)
{
	if (_owner)
		_owner->extents_changed(horz, vert);
}

} // namespace view
} // namespace pv
