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

#ifndef PULSEVIEW_PV_VIEW_TRACETREEITEM_HPP
#define PULSEVIEW_PV_VIEW_TRACETREEITEM_HPP

#include <memory>

#include <QPropertyAnimation>

#include "rowitem.hpp"

namespace pv {
namespace view {

class TraceTreeItemOwner;

class TraceTreeItem : public RowItem,
	public std::enable_shared_from_this<pv::view::TraceTreeItem>
{
	Q_OBJECT
	Q_PROPERTY(int visual_v_offset
		READ visual_v_offset
		WRITE set_visual_v_offset)

public:
	/**
	 * Constructor.
	 */
	TraceTreeItem();

	/**
	 * Gets the owner of this item in the view item hierachy.
	 */
	TraceTreeItemOwner* owner() const;

	/**
	 * Selects or deselects the signal.
	 */
	void select(bool select = true);

	/**
	 * Gets the vertical layout offset of this signal.
	 */
	int layout_v_offset() const;

	/**
	 * Sets the vertical layout offset of this signal.
	 */
	void set_layout_v_offset(int v_offset);

	/**
	 * Gets the vertical visual offset of this signal.
	 */
	int visual_v_offset() const;

	/**
	 * Sets the vertical visual offset of this signal.
	 */
	void set_visual_v_offset(int v_offset);

	/**
	 * Sets the visual and layout offset of this signal.
	 */
	void force_to_v_offset(int v_offset);

	/**
	 * Begins an animation that will animate the visual offset toward
	 * the layout offset.
	 */
	void animate_to_layout_v_offset();

	/**
	 * Sets the owner this trace in the view trace hierachy.
	 * @param The new owner of the trace.
	 */
	void set_owner(pv::view::TraceTreeItemOwner *owner);

	/**
	 * Gets the visual y-offset of the axis.
	 */
	int get_visual_y() const;

	/**
	 * Drags the item to a delta relative to the drag point.
	 * @param delta the offset from the drag point.
	 */
	void drag_by(const QPoint &delta);

	/**
	 * Gets the arrow-tip point of the row item marker.
	 * @param rect the rectangle of the header area.
	 */
	QPoint point(const QRect &rect) const;

	/**
	 * Sets the new background colour state: false = dark, true = bright.
	 * This is to allow for alternating backgrounds but has no effect
	 * when coloured background colours are used.
	 * @param state New bg color state to use.
	 */
	void set_bgcolour_state(bool state);

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	virtual std::pair<int, int> v_extents() const = 0;

protected:
	TraceTreeItemOwner *owner_;

	int layout_v_offset_;
	int visual_v_offset_;

	bool bgcolour_state_;

private:
	QPropertyAnimation v_offset_animation_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_TRACETREEITEM_HPP
