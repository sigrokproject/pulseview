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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_CURSORPAIR_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_CURSORPAIR_HPP

#include "cursor.hpp"

#include <memory>

#include <QPainter>
#include <QRect>

using std::pair;
using std::shared_ptr;

class QPainter;

namespace pv {
namespace views {
namespace trace {

class View;

class CursorPair : public TimeItem
{
	Q_OBJECT

private:
	static const int DeltaPadding;
	static const QColor ViewportFillColor;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 */
	CursorPair(View &view);

	/**
	 * Returns true if the item is visible and enabled.
	 */
	bool enabled() const override;

	/**
	 * Returns a pointer to the first cursor.
	 */
	shared_ptr<Cursor> first() const;

	/**
	 * Returns a pointer to the second cursor.
	 */
	shared_ptr<Cursor> second() const;

	/**
	 * Sets the time of the marker.
	 */
	void set_time(const pv::util::Timestamp& time) override;

	float get_x() const override;

	QPoint drag_point(const QRect &rect) const override;

	pv::widgets::Popup* create_popup(QWidget *parent) override;

public:
	QRectF label_rect(const QRectF &rect) const override;

	/**
	 * Paints the marker's label to the ruler.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the ruler client area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	void paint_label(QPainter &p, const QRect &rect, bool hover) override;

	/**
	 * Paints the background layer of the item with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_back(QPainter &p, ViewItemPaintParams &pp) override;

	/**
	 * Constructs the string to display.
	 */
	QString format_string();

	pair<float, float> get_cursor_offsets() const;

public Q_SLOTS:
	void on_hover_point_changed(const QWidget* widget, const QPoint &hp);

private:
	shared_ptr<Cursor> first_, second_;

	QSizeF text_size_;
	QRectF label_area_;
	bool label_incomplete_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_CURSORPAIR_HPP
