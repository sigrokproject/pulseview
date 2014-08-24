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

#include "selectableitem.h"

class QFormLayout;

namespace pv {
namespace view {

class View;

class Trace : public SelectableItem
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

	void set_view(pv::view::View *view);

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint_mid(QPainter &p, int left, int right);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint_fore(QPainter &p, int left, int right);

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
	 * Gets the y-offset of the axis.
	 */
	int get_y() const;

	/**
	 * Computes the outline rectangle of a label.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(int right);

public:
	virtual void hover_point_changed();

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

Q_SIGNALS:
	void visibility_changed();
	void text_changed();	
	void colour_changed();

protected:
	pv::view::View *_view;

	QString _name;
	QColor _colour;
	int _v_offset;

private:
	pv::widgets::Popup *_popup;
	QFormLayout *_popup_form;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_TRACE_H
