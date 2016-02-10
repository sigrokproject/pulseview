/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_WIDGETS_HIDINGMENUBAR_HPP
#define PULSEVIEW_PV_WIDGETS_HIDINGMENUBAR_HPP

#include <QMenuBar>

namespace pv {
namespace widgets {

/**
 * A menu bar widget that only remains visible while it retains focus.
 */
class HidingMenuBar : public QMenuBar
{
	Q_OBJECT

public:
	/**
	 * Constructor
	 * @param parent The parent widget.
	 */
	HidingMenuBar(QWidget *parent = nullptr);

private:
	/**
	 * Handles the event that the widget loses keyboard focus.
	 * @param e the representation of the event details.
	 */
	void focusOutEvent(QFocusEvent *event);

	/**
	 * Handles the event that a key is depressed.
	 * @param e the representation of the event details.
	 */
	void keyPressEvent(QKeyEvent *event);

private Q_SLOTS:
	/**
	 * Handles a menu items being triggered.
	 */
	void item_triggered();
};

} // widgets
} // pv

#endif // PULSEVIEW_PV_WIDGETS_HIDINGMENUBAR_HPP
