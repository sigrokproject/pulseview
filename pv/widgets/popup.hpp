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

#ifndef PULSEVIEW_PV_WIDGETS_POPUP_HPP
#define PULSEVIEW_PV_WIDGETS_POPUP_HPP

#include <QScrollArea>
#include <QWidget>

namespace pv {
namespace widgets {


// A regular QScrollArea has a fixed size and provides scroll bars when the
// content can't be shown in its entirety. However, we want no horizontal
// scroll bar and want the scroll area to adjust its width to fit the content
// instead.
// Inspired by https://stackoverflow.com/questions/21253755/qscrollarea-with-dynamically-changing-contents?answertab=votes#tab-top
class QWidthAdjustingScrollArea : public QScrollArea
{
	Q_OBJECT

public:
	QWidthAdjustingScrollArea(QWidget* parent = nullptr);
	void setWidget(QWidget* w);
	bool eventFilter(QObject* obj, QEvent* ev);
};


class Popup : public QWidget
{
	Q_OBJECT

public:
	enum Position
	{
		Right,
		Top,
		Left,
		Bottom
	};

private:
	static const unsigned int ArrowLength;
	static const unsigned int ArrowOverlap;
	static const unsigned int MarginWidth;

public:
	Popup(QWidget *parent);

	const QPoint& point() const;
	Position position() const;

	void set_position(const QPoint point, Position pos);

	bool eventFilter(QObject *obj, QEvent *event);

	virtual void show();

private:
	bool space_for_arrow() const;

	QPolygon arrow_polygon() const;

	QRegion arrow_region() const;

	QRect bubble_rect() const;

	QRegion bubble_region() const;

	QRegion popup_region() const;

	void reposition_widget();

private:
	void closeEvent(QCloseEvent*);

	void paintEvent(QPaintEvent*);

	void resizeEvent(QResizeEvent*);

	void mouseReleaseEvent(QMouseEvent *event);

protected:
	void showEvent(QShowEvent *);

Q_SIGNALS:
	void closed();

private:
	QPoint point_;
	Position pos_;
};

} // namespace widgets
} // namespace pv

#endif // PULSEVIEW_PV_WIDGETS_POPUP_HPP
