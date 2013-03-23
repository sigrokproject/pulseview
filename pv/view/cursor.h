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

#ifndef PULSEVIEW_PV_VIEW_CURSOR_H
#define PULSEVIEW_PV_VIEW_CURSOR_H

#include "timemarker.h"

#include <QSizeF>

class QPainter;

namespace pv {
namespace view {

class Cursor : public TimeMarker
{
	Q_OBJECT

private:
	static const QColor LineColour;
	static const QColor FillColour;
	static const QColor HighlightColour;
	static const QColor TextColour;

	static const int Size;
	static const int Offset;

	static const int ArrowSize;

public:
	/**
	 * Constructor.
	 * @param colour A reference to the colour of this cursor.
	 * @param time The time to set the flag to.
	 */
	Cursor(const View &view, double time);

public:
	/**
	 * Gets the marker label rectangle.
	 * @param rect The rectangle of the ruler client area.
	 * @return Returns the label rectangle.
	 */
	QRectF get_label_rect(const QRect &rect) const;

	/**
	 * Paints the cursor's label to the ruler.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the ruler client area.
	 * @param prefix The index of the SI prefix to use.
	 */
	void paint_label(QPainter &p, const QRect &rect,
		unsigned int prefix);

private:
	void compute_text_size(QPainter &p, unsigned int prefix);

private:
	QSizeF _text_size;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_CURSOR_H
