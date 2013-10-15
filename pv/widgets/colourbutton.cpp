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

#include "colourbutton.h"

#include <assert.h>

#include <QApplication>
#include <QPainter>

namespace pv {
namespace widgets {

const int ColourButton::SwatchMargin = 7;

ColourButton::ColourButton(int rows, int cols, QWidget *parent) :
	QPushButton("", parent),
	_popup(rows, cols, this)
{
	connect(this, SIGNAL(clicked(bool)), this, SLOT(on_clicked(bool)));
	connect(&_popup, SIGNAL(selected(int, int)),
		this, SLOT(on_selected(int, int)));
}

ColourPopup& ColourButton::popup()
{
	return _popup;
}

const QColor& ColourButton::colour() const
{
	return _cur_colour;
}

void ColourButton::set_colour(QColor colour)
{
	_cur_colour = colour;

	const unsigned int rows = _popup.well_array().numRows();
	const unsigned int cols = _popup.well_array().numCols();

	for (unsigned int r = 0; r < rows; r++)
		for (unsigned int c = 0; c < cols; c++)
			if (_popup.well_array().cellBrush(r, c).color() ==
				colour)
			{
				_popup.well_array().setSelected(r, c);
				_popup.well_array().setCurrent(r, c);
				return;
			}	
}

void ColourButton::set_palette(const QColor *const palette)
{
	assert(palette);

	const unsigned int rows = _popup.well_array().numRows();
	const unsigned int cols = _popup.well_array().numCols();

	for (unsigned int r = 0; r < rows; r++)
		for (unsigned int c = 0; c < cols; c++)
			_popup.well_array().setCellBrush(r, c,
				QBrush(palette[r * cols + c]));
}

void ColourButton::on_clicked(bool)
{
	_popup.set_position(mapToGlobal(rect().center()), Popup::Bottom);
	_popup.show();
}

void ColourButton::on_selected(int row, int col)
{
	_cur_colour = _popup.well_array().cellBrush(row, col).color();
	selected(_cur_colour);
}

void ColourButton::paintEvent(QPaintEvent *e)
{
	QPushButton::paintEvent(e);

	QPainter p(this);

	const QRect r = rect().adjusted(SwatchMargin, SwatchMargin,
		-SwatchMargin, -SwatchMargin);
	p.setPen(QApplication::palette().color(QPalette::Dark));
	p.setBrush(QBrush(_cur_colour));
	p.drawRect(r);
}

} // widgets
} // pv
