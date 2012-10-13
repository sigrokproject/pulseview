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

#include "signal.h"

#include <boost/shared_ptr.hpp>

namespace pv {

class LogicData;

class LogicSignal : public Signal
{
private:
	static const float Margin;
	static const float Oversampling;

	static const QColor EdgeColour;
	static const QColor HighColour;
	static const QColor LowColour;

	static const QColor LogicSignalColours[10];

public:
	LogicSignal(QString name,
		boost::shared_ptr<LogicData> data,
		int probe_index);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param rect the rectangular area to draw the trace into.
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	void paint(QPainter &p, const QRect &rect, double scale, double offset);

private:

	void paint_caps(QPainter &p, QLineF *const lines,
		std::vector< std::pair<int64_t, bool> > &edges,
		bool level, double samples_per_pixel, double pixels_offset,
		int x_offset, int y_offset);

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

} // namespace pv
