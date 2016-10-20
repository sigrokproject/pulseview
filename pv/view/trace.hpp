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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_TRACE_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_TRACE_HPP

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>

#include <stdint.h>

#include "tracetreeitem.hpp"

#include "pv/data/signalbase.hpp"

class QFormLayout;

namespace pv {

namespace widgets {
class Popup;
}

namespace views {
namespace TraceView {

class Trace : public TraceTreeItem
{
	Q_OBJECT

private:
	static const QPen AxisPen;
	static const int LabelHitPadding;

	static const QColor BrightGrayBGColour;
	static const QColor DarkGrayBGColour;

protected:
	Trace(std::shared_ptr<data::SignalBase> channel);

public:
	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	/**
	 * Set the colour of the signal.
	 */
	virtual void set_colour(QColor colour);

	/**
	 * Enables or disables the coloured background for this trace.
	 */
	void set_coloured_bg(bool state);

	/**
	 * Paints the signal label.
	 * @param p the QPainter to paint into.
	 * @param rect the rectangle of the header area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	virtual void paint_label(QPainter &p, const QRect &rect, bool hover);

	virtual QMenu* create_context_menu(QWidget *parent);

	pv::widgets::Popup* create_popup(QWidget *parent);

	/**
	 * Computes the outline rectangle of a label.
	 * @param rect the rectangle of the header area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(const QRectF &rect) const;

protected:
	/**
	 * Paints the background layer of the signal with a QPainter.
	 * @param p The QPainter to paint into.
	 * @param pp The painting parameters object to paint with.
	 */
	virtual void paint_back(QPainter &p, const ViewItemPaintParams &pp);

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 * @param y the y-offset of the axis.
	 */
	void paint_axis(QPainter &p, const ViewItemPaintParams &pp, int y);

	void add_colour_option(QWidget *parent, QFormLayout *form);

	void create_popup_form();

	virtual void populate_popup_form(QWidget *parent, QFormLayout *form);

protected Q_SLOTS:
	virtual void on_name_changed(const QString &text);

	virtual void on_colour_changed(const QColor &colour);

	void on_popup_closed();

private Q_SLOTS:
	void on_nameedit_changed(const QString &name);

	void on_colouredit_changed(const QColor &colour);

protected:
	std::shared_ptr<data::SignalBase> base_;
	bool coloured_bg_, coloured_bg_state_;

private:
	pv::widgets::Popup *popup_;
	QFormLayout *popup_form_;
};

} // namespace TraceView
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_TRACE_HPP
