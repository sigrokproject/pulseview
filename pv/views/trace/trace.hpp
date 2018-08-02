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

#include <cstdint>

#include "tracetreeitem.hpp"

#include <pv/globalsettings.hpp>
#include "pv/data/signalbase.hpp"

using std::shared_ptr;

class QFormLayout;

namespace pv {

namespace data {
class SignalBase;
}

namespace widgets {
class Popup;
}

namespace views {
namespace trace {

/**
 * The Trace class represents a @ref TraceTreeItem which occupies some vertical
 * space on the canvas and spans across its entire width, essentially showing
 * a time series of values, events, objects or similar. While easily confused
 * with @ref Signal, the difference is that Trace may represent anything that
 * can be drawn, not just numeric values. One example is a @ref DecodeTrace.
 *
 * For this reason, Trace is more generic and contains properties and helpers
 * that benefit any kind of time series items.
 */
class Trace : public TraceTreeItem, public GlobalSettingsInterface
{
	Q_OBJECT

public:
	/**
	 * Allowed values for the multi-segment display mode.
	 *
	 * Note: Consider these locations when updating the list:
	 * *
	 * @ref View::set_segment_display_mode
	 * @ref View::on_segment_changed
	 * @ref AnalogSignal::get_analog_segment_to_paint
	 * @ref AnalogSignal::get_logic_segment_to_paint
	 * @ref LogicSignal::get_logic_segment_to_paint
	 * @ref StandardBar
	 */
	enum SegmentDisplayMode {
		ShowLastSegmentOnly = 1,
		ShowLastCompleteSegmentOnly,
		ShowSingleSegmentOnly,
		ShowAllSegments,
		ShowAccumulatedIntensity
	};

private:
	static const QPen AxisPen;
	static const int LabelHitPadding;

	static const QColor BrightGrayBGColor;
	static const QColor DarkGrayBGColor;

protected:
	Trace(shared_ptr<data::SignalBase> channel);
	~Trace();

public:
	/**
	 * Returns the underlying SignalBase instance.
	 */
	shared_ptr<data::SignalBase> base() const;

	/**
	 * Configures the segment display mode to use.
	 */
	virtual void set_segment_display_mode(SegmentDisplayMode mode);

	virtual void on_setting_changed(const QString &key, const QVariant &value);

	/**
	 * Paints the signal label.
	 * @param p the QPainter to paint into.
	 * @param rect the rectangle of the header area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	virtual void paint_label(QPainter &p, const QRect &rect, bool hover);

	virtual QMenu* create_header_context_menu(QWidget *parent);

	pv::widgets::Popup* create_popup(QWidget *parent);

	/**
	 * Computes the outline rectangle of a label.
	 * @param rect the rectangle of the header area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(const QRectF &rect) const;

	/**
	 * Computes the outline rectangle of the viewport hit-box.
	 * @param rect the rectangle of the viewport area.
	 * @return Returns the rectangle of the hit-box.
	 * @remarks The default implementation returns an empty hit-box.
	 */
	virtual QRectF hit_box_rect(const ViewItemPaintParams &pp) const;

	void set_current_segment(const int segment);

	int get_current_segment() const;

	virtual void hover_point_changed(const QPoint &hp);

protected:
	/**
	 * Paints the background layer of the signal with a QPainter.
	 * @param p The QPainter to paint into.
	 * @param pp The painting parameters object to paint with.
	 */
	virtual void paint_back(QPainter &p, ViewItemPaintParams &pp);

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 * @param y the y-offset of the axis.
	 */
	void paint_axis(QPainter &p, ViewItemPaintParams &pp, int y);

	/**
	 * Draw a hover marker under the cursor position.
	 * @param p The painter to draw into.
	 */
	void paint_hover_marker(QPainter &p);

	void add_color_option(QWidget *parent, QFormLayout *form);

	void create_popup_form();

	virtual void populate_popup_form(QWidget *parent, QFormLayout *form);

protected Q_SLOTS:
	virtual void on_name_changed(const QString &text);

	virtual void on_color_changed(const QColor &color);

	void on_popup_closed();

private Q_SLOTS:
	void on_nameedit_changed(const QString &name);

	void on_coloredit_changed(const QColor &color);

protected:
	shared_ptr<data::SignalBase> base_;
	QPen axis_pen_;

	SegmentDisplayMode segment_display_mode_;
	bool show_hover_marker_;

	/// The ID of the currently displayed segment
	int current_segment_;

private:
	pv::widgets::Popup *popup_;
	QFormLayout *popup_form_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_TRACE_HPP
