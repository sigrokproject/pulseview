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

#ifndef PULSEVIEW_PV_VIEW_TRACE_H
#define PULSEVIEW_PV_VIEW_TRACE_H

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>

#include <stdint.h>

#include "rowitem.h"

class QFormLayout;

namespace pv {

namespace widgets {
class Popup;
}

namespace view {

class Trace : public RowItem
{
	Q_OBJECT

private:
	static const QPen AxisPen;
	static const int LabelHitPadding;

protected:
	Trace(QString name);

public:
	/**
	 * Gets the name of this signal.
	 */
	QString name() const;

	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	/**
	 * Get the colour of the signal.
	 */
	QColor colour() const;

	/**
	 * Set the colour of the signal.
	 */
	void set_colour(QColor colour);

	/**
	 * Paints the signal label.
	 * @param p the QPainter to paint into.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	virtual void paint_label(QPainter &p, int right, bool hover);

	virtual QMenu* create_context_menu(QWidget *parent);

	pv::widgets::Popup* create_popup(QWidget *parent);

	/**
	 * Computes the outline rectangle of a label.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(int right) const;

protected:

	/**
	 * Gets the text colour.
	 * @remarks This colour is computed by comparing the lightness
	 * of the trace colour against a threshold to determine whether
	 * white or black would be more visible.
	 */
	QColor get_text_colour() const;

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param y the y-offset of the axis.
	 * @param left the x-coordinate of the left edge of the view.
	 * @param right the x-coordinate of the right edge of the view.
	 */
	void paint_axis(QPainter &p, int y, int left, int right);

	void add_colour_option(QWidget *parent, QFormLayout *form);

	void create_popup_form();

	virtual void populate_popup_form(QWidget *parent, QFormLayout *form);

private Q_SLOTS:
	void on_text_changed(const QString &text);

	void on_colour_changed(const QColor &colour);

	void on_popup_closed();

protected:
	QString name_;
	QColor colour_;

private:
	pv::widgets::Popup *popup_;
	QFormLayout *popup_form_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_TRACE_H
