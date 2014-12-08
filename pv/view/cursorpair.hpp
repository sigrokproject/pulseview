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

#ifndef PULSEVIEW_PV_VIEW_CURSORPAIR_H
#define PULSEVIEW_PV_VIEW_CURSORPAIR_H

#include "cursor.hpp"

#include <memory>

#include <QPainter>

class QPainter;

namespace pv {
namespace view {

class CursorPair : public TimeItem
{
private:
	static const int DeltaPadding;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 */
	CursorPair(View &view);

public:
	/**
	 * Returns true if the item is visible and enabled.
	 */
	bool enabled() const;

	/**
	 * Returns a pointer to the first cursor.
	 */
	std::shared_ptr<Cursor> first() const;

	/**
	 * Returns a pointer to the second cursor.
	 */
	std::shared_ptr<Cursor> second() const;

	/**
	 * Sets the time of the marker.
	 */
	void set_time(double time);

	float get_x() const;

	QPoint point() const;

	pv::widgets::Popup* create_popup(QWidget *parent);

public:
	QRectF label_rect(const QRectF &rect) const;

	void draw_markers(QPainter &p, const QRect &rect);

	void draw_viewport_background(QPainter &p, const QRect &rect);

	void draw_viewport_foreground(QPainter &p, const QRect &rect);

	void compute_text_size(QPainter &p, unsigned int prefix);

	std::pair<float, float> get_cursor_offsets() const;

private:
	std::shared_ptr<Cursor> first_, second_;

	QSizeF text_size_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_CURSORPAIR_H
