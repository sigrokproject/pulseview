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

#ifndef PULSEVIEW_PV_WIDGETS_COLOURBUTTON_HPP
#define PULSEVIEW_PV_WIDGETS_COLOURBUTTON_HPP

#include <QPushButton>

#include "colourpopup.hpp"

namespace pv {
namespace widgets {

class ColourButton : public QPushButton
{
	Q_OBJECT;

private:
	static const int SwatchMargin;

public:
	ColourButton(int rows, int cols, QWidget *parent);

	ColourPopup& popup();

	const QColor& colour() const;

	void set_colour(QColor colour);

	void set_palette(const QColor *const palette);

private:
	void paintEvent(QPaintEvent *event);

private Q_SLOTS:
	void on_clicked(bool);

	void on_selected(int row, int col);

Q_SIGNALS:
	void selected(const QColor &colour);

private:
	ColourPopup popup_;
	QColor cur_colour_;
};

} // widgets
} // pv

#endif // PULSEVIEW_PV_WIDGETS_COLOURBUTTON_HPP
