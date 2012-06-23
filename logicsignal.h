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

#include "signal.h"

#include <boost/shared_ptr.hpp>

class LogicData;

class LogicSignal : public Signal
{
private:
	struct Point2F
	{
		GLfloat x, y;
	};

private:
	static const float Margin;

	static const float EdgeColour[3];
	static const float HighColour[3];
	static const float LowColour[3];

	static const QColor LogicSignalColours[10];

public:
	LogicSignal(QString name,
		boost::shared_ptr<LogicData> data,
		int probe_index);

	/**
	 * Paints the signal into a QGLWidget.
	 * @param widget the QGLWidget to paint into.
	 * @param rect the rectangular area to draw the trace into.
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	void paint(QGLWidget &widget, const QRect &rect, double scale,
		double offset);

private:

	int paint_caps(Point2F *const cap_points,
		std::vector< std::pair<int64_t, bool> > &edges,
		bool level, double samples_per_pixel, double pixels_offset,
		int x_offset, int y_offset);

	static void paint_lines(Point2F *points, int count);

	/**
	 * Get the colour of the logic signal
	 */
	QColor get_colour() const;

	/**
	 * When painting into the rectangle, calculate the y
	 * offset of the zero point.
	 **/
	int get_nominal_offset(const QRect &rect) const;

private:
	int _probe_index;
	boost::shared_ptr<LogicData> _data;
};
