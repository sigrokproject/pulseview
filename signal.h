/*
 * This file is part of the sigrok project.
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

#include <boost/shared_ptr.hpp>

#include <QGLWidget>
#include <QPainter>
#include <QRect>
#include <QString>

#include <stdint.h>

class SignalData;

class Signal
{
private:
	static const QSizeF LabelPadding;

protected:
	Signal(QString name);

public:
	QString get_name() const;

	/**
	 * Paints the signal into a QGLWidget.
	 * @param widget the QGLWidget to paint into.
	 * @param rect the rectangular area to draw the trace into.
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	virtual void paint(QGLWidget &widget, const QRect &rect,
		double scale, double offset) = 0;

	/**
	 * Paints the signal label into a QGLWidget.
	 * @param p the QPainter to paint into.
	 * @param rect the rectangular area to draw the label into.
	 */
	virtual void paint_label(QPainter &p, const QRect &rect);

protected:

	/**
	 * Get the colour of the logic signal
	 */
	virtual QColor get_colour() const = 0;

	/**
	 * When painting into the rectangle, calculate the y
	 * offset of the zero point.
	 **/
	virtual int get_nominal_offset(const QRect &rect) const = 0;

protected:
	QString _name;
};
