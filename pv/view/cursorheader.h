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

#ifndef PULSEVIEW_PV_VIEW_CURSORHEADER_H
#define PULSEVIEW_PV_VIEW_CURSORHEADER_H

#include <memory>

#include "marginwidget.h"

namespace pv {
namespace view {

class TimeMarker;

/**
 * Widget to hold the labels over the cursors.
 */
class CursorHeader : public MarginWidget
{
	Q_OBJECT

	static const int Padding;

	/**
	 * The vertical offset, relative to the bottom line of the widget,
	 * where the arrows of the cursor labels end.
	 */
	static const int BaselineOffset;

public:
	CursorHeader(View &parent);

	QSize sizeHint() const;

	void clear_selection();

private:
	void paintEvent(QPaintEvent *event);

	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *);

	int calculateTextHeight();

	std::weak_ptr<TimeMarker> grabbed_marker_;
	QPoint mouse_down_point_;
	bool dragging_;
	const int textHeight_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_CURSORHEADER_H
