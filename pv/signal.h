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

#ifndef PULSEVIEW_PV_SIGNAL_H
#define PULSEVIEW_PV_SIGNAL_H

#include <boost/shared_ptr.hpp>

#include <QColor>
#include <QPainter>
#include <QRect>
#include <QString>

#include <stdint.h>

namespace pv {

class SignalData;

class Signal
{
private:
	static const int LabelHitPadding;

protected:
	Signal(QString name);

public:
	/**
	 * Gets the name of this signal.
	 */
	QString get_name() const;

	/**
	 * Sets the name of the signal.
	 */
	void set_name(QString name);

	/**
	 * Get the colour of the signal.
	 */
	QColor get_colour() const;

	/**
	 * Set the colour of the signal.
	 */
	void set_colour(QColor colour);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param rect the rectangular area to draw the trace into.
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	virtual void paint(QPainter &p, const QRect &rect, double scale,
		double offset) = 0;


	/**
	 * Paints the signal label into a QGLWidget.
	 * @param p the QPainter to paint into.
	 * @param rect the rectangular area to draw the label into.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	virtual void paint_label(QPainter &p, const QRect &rect,
		bool hover);

	/**
	 * Determines if a point is in the header label rect.
	 * @param rect the rectangular area to draw the label into.
	 * @param point the point to test.
	 */
	bool pt_in_label_rect(const QRect &rect, const QPoint &point);

private:

	/**
	 * Computes an caches the size of the label text.
	 */
	void compute_text_size(QPainter &p);

	/**
	 * Computes the outline rectangle of a label.
	 * @param p the QPainter to lay out text with.
	 * @param rect The rectangle of the signal header.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF get_label_rect(const QRect &rect);

protected:
	/**
	 * When painting into the rectangle, calculate the y
	 * offset of the zero point.
	 **/
	virtual int get_nominal_offset(const QRect &rect) const = 0;

protected:
	QString _name;
	QColor _colour;

	QSizeF _text_size;
};

} // namespace pv

#endif // PULSEVIEW_PV_SIGNAL_H
