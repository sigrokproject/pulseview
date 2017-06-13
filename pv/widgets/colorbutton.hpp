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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_WIDGETS_COLORBUTTON_HPP
#define PULSEVIEW_PV_WIDGETS_COLORBUTTON_HPP

#include <QPushButton>

#include "colorpopup.hpp"

namespace pv {
namespace widgets {

class ColorButton : public QPushButton
{
	Q_OBJECT;

private:
	static const int SwatchMargin;

public:
	/**
	 * Construct a ColorButton instance that uses a QColorDialog
	 */
	ColorButton(QWidget *parent);

	/**
	 * Construct a ColorButton instance that uses a ColorPopup
	 */
	ColorButton(int rows, int cols, QWidget *parent);

	ColorPopup* popup();

	const QColor& color() const;

	void set_color(QColor color);

	void set_palette(const QColor *const palette);

private:
	void paintEvent(QPaintEvent *event);

private Q_SLOTS:
	void on_clicked(bool);
	void on_selected(int row, int col);
	void on_color_selected(const QColor& color);

Q_SIGNALS:
	void selected(const QColor &color);

private:
	ColorPopup* popup_;
	QColor cur_color_;
};

}  // namespace widgets
}  // namespace pv

#endif // PULSEVIEW_PV_WIDGETS_COLORBUTTON_HPP
