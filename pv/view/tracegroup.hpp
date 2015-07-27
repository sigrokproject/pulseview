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

#ifndef PULSEVIEW_PV_VIEW_TRACEGROUP_HPP
#define PULSEVIEW_PV_VIEW_TRACEGROUP_HPP

#include "tracetreeitem.hpp"
#include "tracetreeitemowner.hpp"

namespace pv {
namespace view {

class TraceGroup : public TraceTreeItem, public TraceTreeItemOwner
{
	Q_OBJECT

private:
	static const int Padding;
	static const int Width;
	static const int LineThickness;
	static const QColor LineColour;

public:
	/**
	 * Virtual destructor
	 */
	virtual ~TraceGroup();

	/**
	 * Returns true if the item is visible and enabled.
	 */
	bool enabled() const;

	/**
	 * Returns the session of the onwer.
	 */
	pv::Session& session();

	/**
	 * Returns the session of the onwer.
	 */
	const pv::Session& session() const;

	/**
	 * Returns the view of the owner.
	 */
	virtual pv::view::View* view();

	/**
	 * Returns the view of the owner.
	 */
	virtual const pv::view::View* view() const;

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	std::pair<int, int> v_extents() const;

	/**
	 * Paints the signal label.
	 * @param p the QPainter to paint into.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	void paint_label(QPainter &p, const QRect &rect, bool hover);

	/**
	 * Computes the outline rectangle of a label.
	 * @param rect the rectangle of the header area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(const QRectF &rect) const;

	/**
	 * Determines if a point is in the header label rect.
	 * @param left the x-coordinate of the left edge of the header
	 * 	area.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param point the point to test.
	 */
	bool pt_in_label_rect(int left, int right, const QPoint &point);

	QMenu* create_context_menu(QWidget *parent);

	pv::widgets::Popup* create_popup(QWidget *parent);

	/**
	 * Returns the total vertical offset of this trace and all it's owners
	 */
	int owner_visual_v_offset() const;

	void restack_items();

	/**
	 * Returns the number of nested parents that this row item owner has.
	 */
	unsigned int depth() const;

	void ungroup();

public:
	void row_item_appearance_changed(bool label, bool content);

	void extents_changed(bool horz, bool vert);

private Q_SLOTS:
	void on_ungroup();
};

} // view
} // pv

#endif // PULSEVIEW_PV_VIEW_TRACEGROUP_HPP
