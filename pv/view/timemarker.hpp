/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_VIEW_MARKER_H
#define PULSEVIEW_PV_VIEW_MARKER_H

#include <QColor>
#include <QDoubleSpinBox>
#include <QObject>
#include <QRectF>
#include <QWidgetAction>

#include "timeitem.hpp"

class QPainter;
class QRect;

namespace pv {
namespace view {

class View;

class TimeMarker : public TimeItem
{
	Q_OBJECT

public:
	static const int ArrowSize;
	static const int Offset;

protected:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this marker.
	 * @param colour A reference to the colour of this cursor.
	 * @param time The time to set the flag to.
	 */
	TimeMarker(View &view, const QColor &colour, double time);

public:
	/**
	 * Gets the time of the marker.
	 */
	double time() const;

	/**
	 * Sets the time of the marker.
	 */
	void set_time(double time);

	float get_x() const;

	/**
	 * Gets the drag point of the row item.
	 */
	QPoint point() const;

	/**
	 * Computes the outline rectangle of a label.
	 * @param rect the rectangle of the header area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(const QRectF &rect) const;

	/**
	 * Gets the text to show in the marker.
	 */
	virtual QString get_text() const = 0;

	/**
	 * Paints the marker's label to the ruler.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the ruler client area.
	 */
	void paint_label(QPainter &p, const QRect &rect);

	/**
	 * Paints the foreground layer of the item with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 **/
	void paint_fore(QPainter &p, const ViewItemPaintParams &pp);

	pv::widgets::Popup* create_popup(QWidget *parent);

private Q_SLOTS:
	void on_value_changed(double value);

protected:
	const QColor &colour_;

	double time_;

	QSizeF text_size_;

	QWidgetAction *value_action_;
	QDoubleSpinBox *value_widget_;
	bool updating_value_widget_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_MARKER_H
