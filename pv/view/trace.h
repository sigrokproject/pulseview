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

#ifndef PULSEVIEW_PV_TRACE_H
#define PULSEVIEW_PV_TRACE_H

#include <QColor>
#include <QPainter>
#include <QRect>
#include <QString>

#include <stdint.h>

#include "selectableitem.h"

namespace pv {

class SigSession;

namespace view {

class View;

class Trace : public SelectableItem
{
	Q_OBJECT

private:
	static const int LabelHitPadding;

protected:
	Trace(SigSession &session, QString name);

public:
	/**
	 * Gets the name of this signal.
	 */
	QString get_name() const;

	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	/**
	 * Get the colour of the signal.
	 */
	QColor get_colour() const;

	/**
	 * Set the colour of the signal.
	 */
	void set_colour(QColor colour);

	/**
	 * Gets the vertical layout offset of this signal.
	 */
	int get_v_offset() const;

	/**
	 * Sets the vertical layout offset of this signal.
	 */
	void set_v_offset(int v_offset);

	/**
	 * Returns true if the trace is visible and enabled.
	 */
	virtual bool enabled() const = 0;

	virtual void set_view(pv::view::View *view);

	/**
	 * Paints the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint(QPainter &p, int left, int right) = 0;

	/**
	 * Paints the signal label into a QGLWidget.
	 * @param p the QPainter to paint into.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	virtual void paint_label(QPainter &p, int right, bool hover);

	/**
	 * Determines if a point is in the header label rect.
	 * @param left the x-coordinate of the left edge of the header
	 * 	area.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param point the point to test.
	 */
	bool pt_in_label_rect(int left, int right, const QPoint &point);

private:

	/**
	 * Computes an caches the size of the label text.
	 */
	void compute_text_size(QPainter &p);

	/**
	 * Computes the outline rectangle of a label.
	 * @param p the QPainter to lay out text with.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF get_label_rect(int right);

signals:
	void text_changed();	

protected:
	pv::SigSession &_session;
	pv::view::View *_view;

	QString _name;
	QColor _colour;
	int _v_offset;

	QSizeF _text_size;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_TRACEL_H
