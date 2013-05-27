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

#ifndef PULSEVIEW_PV_SIGNAL_H
#define PULSEVIEW_PV_SIGNAL_H

#include <boost/shared_ptr.hpp>

#include <QColor>
#include <QComboBox>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>
#include <QWidgetAction>

#include <stdint.h>

#include <libsigrok/libsigrok.h>

#include "selectableitem.h"

namespace pv {

class SigSession;

namespace data {
class SignalData;
}

namespace view {

class Signal : public SelectableItem
{
	Q_OBJECT

private:
	static const int LabelHitPadding;

	static const QPen SignalAxisPen;

protected:
	Signal(pv::SigSession &session, const sr_probe *const probe);

public:
	/**
	 * Gets the name of this signal.
	 */
	QString get_name() const;

	/**
	 * Sets the name of the signal.
	 */
	void set_name(QString name);

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
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param y the y-coordinate to draw the signal at
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	virtual void paint(QPainter &p, int y, int left, int right,
		double scale, double offset) = 0;

	/**
	 * Paints the signal label into a QGLWidget.
	 * @param p the QPainter to paint into.
	 * @param y the y-coordinate of the signal.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	virtual void paint_label(QPainter &p, int y, int right,
		bool hover);

	/**
	 * Determines if a point is in the header label rect.
	 * @param y the y-coordinate of the signal.
	 * @param left the x-coordinate of the left edge of the header
	 * 	area.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param point the point to test.
	 */
	bool pt_in_label_rect(int y, int left, int right,
		const QPoint &point);

protected:

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param y the y-offset of the axis.
	 * @param left the x-coordinate of the left edge of the view.
	 * @param right the x-coordinate of the right edge of the view.
	 */
	void paint_axis(QPainter &p, int y, int left, int right);

private:

	/**
	 * Computes an caches the size of the label text.
	 */
	void compute_text_size(QPainter &p);

	/**
	 * Computes the outline rectangle of a label.
	 * @param p the QPainter to lay out text with.
	 * @param y the y-coordinate of the signal.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF get_label_rect(int y, int right);

private slots:
	void on_text_changed(const QString &text);

signals:
	void text_changed();	

protected:
	pv::SigSession &_session;
	const sr_probe *const _probe;

	QString _name;
	QColor _colour;
	int _v_offset;

	QSizeF _text_size;

	QWidgetAction _name_action;
	QComboBox _name_widget;
	bool _updating_name_widget;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_SIGNAL_H
