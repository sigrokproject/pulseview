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

#ifndef PULSEVIEW_PV_VIEW_DECODE_ANNOTATION_H
#define PULSEVIEW_PV_VIEW_DECODE_ANNOTATION_H

#include <stdint.h>

#include <QString>

struct srd_proto_data;

namespace pv {
namespace view {
namespace decode {

class Annotation
{
private:
	static const double EndCapWidth;
	static const int DrawPadding;

	static const QColor Colours[7];

public:
	Annotation(const srd_proto_data *const pdata);

	void paint(QPainter &p, QColor text_colour, int text_height, int left,
		int right, double samples_per_pixel, double pixels_offset,
		int y);

private:
	void draw_instant(QPainter &p, QColor fill, QColor outline,
		QColor text_color, int h, double x, int y);

	void draw_range(QPainter &p, QColor fill, QColor outline,
		QColor text_color, int h, double start,
		double end, int y);

private:
	uint64_t _start_sample;
	uint64_t _end_sample;
	int _format;
	std::vector<QString> _annotations; 
};

} // namespace decode
} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_DECODE_ANNOTATION_H
