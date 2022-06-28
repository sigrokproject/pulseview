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

#include <cassert>
#include <QMenu>

#include "view.hpp"

#include "tracetreeitem.hpp"

namespace pv {
namespace views {
namespace trace {

TraceTreeItem::TraceTreeItem() :
	owner_(nullptr),
	layout_v_offset_(0),
	visual_v_offset_(0),
	v_offset_animation_(this, "visual_v_offset")
{
}

void TraceTreeItem::select(bool select)
{
	ViewItem::select(select);
	owner_->row_item_appearance_changed(true, true);
}

int TraceTreeItem::layout_v_offset() const
{
	return layout_v_offset_;
}

void TraceTreeItem::set_layout_v_offset(int v_offset)
{
	if (layout_v_offset_ == v_offset)
		return;

	layout_v_offset_ = v_offset;

	if (owner_)
		owner_->extents_changed(false, true);
}

int TraceTreeItem::visual_v_offset() const
{
	return visual_v_offset_;
}

void TraceTreeItem::set_visual_v_offset(int v_offset)
{
	visual_v_offset_ = v_offset;

	if (owner_)
		owner_->row_item_appearance_changed(true, true);
}

void TraceTreeItem::force_to_v_offset(int v_offset)
{
	v_offset_animation_.stop();
	layout_v_offset_ = visual_v_offset_ = v_offset;

	if (owner_) {
		owner_->row_item_appearance_changed(true, true);
		owner_->extents_changed(false, true);
	}
}

void TraceTreeItem::animate_to_layout_v_offset()
{
	if (visual_v_offset_ == layout_v_offset_ ||
		(v_offset_animation_.endValue() == layout_v_offset_ &&
		v_offset_animation_.state() == QAbstractAnimation::Running))
		return;

	v_offset_animation_.setDuration(100);
	v_offset_animation_.setStartValue(visual_v_offset_);
	v_offset_animation_.setEndValue(layout_v_offset_);
	v_offset_animation_.setEasingCurve(QEasingCurve::OutQuad);
	v_offset_animation_.start();
}

TraceTreeItemOwner* TraceTreeItem::owner() const
{
	return owner_;
}

void TraceTreeItem::set_owner(TraceTreeItemOwner *owner)
{
	assert(owner_ || owner);
	v_offset_animation_.stop();

	if (owner_) {
		const int owner_offset = owner_->owner_visual_v_offset();
		layout_v_offset_ += owner_offset;
		visual_v_offset_ += owner_offset;
	}

	owner_ = owner;

	if (owner_) {
		const int owner_offset = owner_->owner_visual_v_offset();
		layout_v_offset_ -= owner_offset;
		visual_v_offset_ -= owner_offset;
	}
}

int TraceTreeItem::get_visual_y() const
{
	assert(owner_);
	return visual_v_offset_ + owner_->owner_visual_v_offset();
}

void TraceTreeItem::drag_by(const QPoint &delta)
{
	assert(owner_);
	force_to_v_offset(drag_point_.y() + delta.y() -
		owner_->owner_visual_v_offset());
}

QPoint TraceTreeItem::drag_point(const QRect &rect) const
{
	return QPoint(rect.right(), get_visual_y());
}

QMenu* TraceTreeItem::create_view_context_menu(QWidget *parent, QPoint &click_pos)
{
    (void)click_pos;

    QMenu *const menu = new QMenu(parent);

    View *view = owner_->view();

    if(view->scale() != 1e-3) {
        QAction *const reset_zoom = new QAction(tr("Reset zoom"), this);
        connect(reset_zoom, SIGNAL(triggered()), this, SLOT(on_zoom_reset()));
        menu->addAction(reset_zoom);
    }

    return menu;
}

void TraceTreeItem::on_zoom_reset()
{
    View *view = owner_->view();
    assert(view);
    view->reset_zoom();
}

} // namespace trace
} // namespace views
} // namespace pv
