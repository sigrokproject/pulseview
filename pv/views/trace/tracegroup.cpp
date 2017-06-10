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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <extdef.h>

#include <algorithm>
#include <cassert>

#include <QMenu>
#include <QPainter>

#include "tracegroup.hpp"

using std::any_of;
using std::pair;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace views {
namespace trace {

const int TraceGroup::Padding = 8;
const int TraceGroup::Width = 12;
const int TraceGroup::LineThickness = 5;
const QColor TraceGroup::LineColour(QColor(0x55, 0x57, 0x53));

TraceGroup::~TraceGroup()
{
	owner_ = nullptr;
	clear_child_items();
}

bool TraceGroup::enabled() const
{
	return any_of(child_items().begin(), child_items().end(),
		[](const shared_ptr<ViewItem> &r) { return r->enabled(); });
}

pv::Session& TraceGroup::session()
{
	assert(owner_);
	return owner_->session();
}

const pv::Session& TraceGroup::session() const
{
	assert(owner_);
	return owner_->session();
}

View* TraceGroup::view()
{
	assert(owner_);
	return owner_->view();
}

const View* TraceGroup::view() const
{
	assert(owner_);
	return owner_->view();
}

pair<int, int> TraceGroup::v_extents() const
{
	return TraceTreeItemOwner::v_extents();
}

void TraceGroup::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	const QRectF r = label_rect(rect).adjusted(
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

QRectF TraceGroup::label_rect(const QRectF &rect) const
{
	QRectF child_rect;
	for (const shared_ptr<ViewItem> r : child_items())
		if (r && r->enabled())
			child_rect = child_rect.united(r->label_rect(rect));

	return QRectF(child_rect.x() - Width - Padding, child_rect.y(),
		Width, child_rect.height());
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
	return nullptr;
}

int TraceGroup::owner_visual_v_offset() const
{
	return owner_ ? visual_v_offset() + owner_->owner_visual_v_offset() : 0;
}

void TraceGroup::restack_items()
{
	vector<shared_ptr<TraceTreeItem>> items(trace_tree_child_items());

	// Sort by the centre line of the extents
	stable_sort(items.begin(), items.end(),
		[](const shared_ptr<TraceTreeItem> &a, const shared_ptr<TraceTreeItem> &b) {
			const auto aext = a->v_extents();
			const auto bext = b->v_extents();
			return a->layout_v_offset() +
					(aext.first + aext.second) / 2 <
				b->layout_v_offset() +
					(bext.first + bext.second) / 2;
		});

	int total_offset = 0;
	for (shared_ptr<TraceTreeItem> r : items) {
		const pair<int, int> extents = r->v_extents();
		if (extents.first == 0 && extents.second == 0)
			continue;

		// We position disabled traces, so that they are close to the
		// animation target positon should they be re-enabled
		if (r->enabled())
			total_offset += -extents.first;

		if (!r->dragging())
			r->set_layout_v_offset(total_offset);

		if (r->enabled())
			total_offset += extents.second;
	}
}

unsigned int TraceGroup::depth() const
{
	return owner_ ? owner_->depth() + 1 : 0;
}

void TraceGroup::ungroup()
{
	const vector<shared_ptr<TraceTreeItem>> items(trace_tree_child_items());
	clear_child_items();

	for (shared_ptr<TraceTreeItem> r : items)
		owner_->add_child_item(r);

	owner_->remove_child_item(shared_from_this());
}

void TraceGroup::on_ungroup()
{
	ungroup();
}

void TraceGroup::row_item_appearance_changed(bool label, bool content)
{
	if (owner_)
		owner_->row_item_appearance_changed(label, content);
}

void TraceGroup::extents_changed(bool horz, bool vert)
{
	if (owner_)
		owner_->extents_changed(horz, vert);
}

} // namespace trace
} // namespace views
} // namespace pv
