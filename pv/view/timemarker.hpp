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

#ifndef PULSEVIEW_PV_VIEW_MARKER_HPP
#define PULSEVIEW_PV_VIEW_MARKER_HPP

#include <QColor>
#include <QDoubleSpinBox>
#include <QObject>
#include <QRectF>
#include <QWidgetAction>

#include "timeitem.hpp"

class QPainter;
class QRect;

namespace pv {
namespace widgets {
	class TimestampSpinBox;
}

namespace view {

class View;

class TimeMarker : public TimeItem
{
	Q_OBJECT

public:
	static const int ArrowSize;

protected:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this marker.
	 * @param colour A reference to the colour of this cursor.
	 * @param time The time to set the flag to.
	 */
	TimeMarker(View &view, const QColor &colour, const pv::util::Timestamp& time);

public:
	/**
	 * Gets the time of the marker.
	 */
	const pv::util::Timestamp& time() const;

	/**
	 * Sets the time of the marker.
	 */
	void set_time(const pv::util::Timestamp& time) override;

	float get_x() const override;

	/**
	 * Gets the arrow-tip point of the time marker.
	 * @param rect the rectangle of the ruler area.
	 */
	QPoint point(const QRect &rect) const override;

	/**
	 * Computes the outline rectangle of a label.
	 * @param rect the rectangle of the header area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(const QRectF &rect) const override;

	/**
	 * Computes the outline rectangle of the viewport hit-box.
	 * @param rect the rectangle of the viewport area.
	 * @return Returns the rectangle of the hit-box.
	 */
	QRectF hit_box_rect(const ViewItemPaintParams &pp) const override;

	/**
	 * Gets the text to show in the marker.
	 */
	virtual QString get_text() const = 0;

	/**
	 * Paints the marker's label to the ruler.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the ruler client area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	void paint_label(QPainter &p, const QRect &rect, bool hover) override;

	/**
	 * Paints the foreground layer of the item with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_fore(QPainter &p, const ViewItemPaintParams &pp) override;

	virtual pv::widgets::Popup* create_popup(QWidget *parent) override;

private Q_SLOTS:
	void on_value_changed(const pv::util::Timestamp& value);

protected:
	const QColor &colour_;

	pv::util::Timestamp time_;

	QSizeF text_size_;

	QWidgetAction *value_action_;
	pv::widgets::TimestampSpinBox *value_widget_;
	bool updating_value_widget_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_MARKER_HPP
