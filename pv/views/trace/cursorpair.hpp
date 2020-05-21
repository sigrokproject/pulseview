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

#ifndef PULSEVIEW_PV_VIEWS_TRACE_CURSORPAIR_HPP
#define PULSEVIEW_PV_VIEWS_TRACE_CURSORPAIR_HPP

#include "cursor.hpp"
#include "pv/globalsettings.hpp"

#include <memory>

#include <QColor>
#include <QPainter>
#include <QRect>

using std::pair;
using std::shared_ptr;

class QPainter;

namespace pv {
namespace views {
namespace trace {

class View;

class CursorPair : public TimeItem, public GlobalSettingsInterface
{
	Q_OBJECT

private:
	static const int DeltaPadding;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 */
	CursorPair(View &view);

	~CursorPair();

	/**
	 * Returns true if the item is visible and enabled.
	 */
	bool enabled() const override;

	/**
	 * Returns a pointer to the first cursor.
	 */
	shared_ptr<Cursor> first() const;

	/**
	 * Returns a pointer to the second cursor.
	 */
	shared_ptr<Cursor> second() const;

	/**
	 * Sets the time of the marker.
	 */
	void set_time(const pv::util::Timestamp& time) override;

	virtual const pv::util::Timestamp time() const override;

	float get_x() const override;

	virtual const pv::util::Timestamp delta(const pv::util::Timestamp& other) const override;

	QPoint drag_point(const QRect &rect) const override;

	pv::widgets::Popup* create_popup(QWidget *parent) override;

	QMenu* create_header_context_menu(QWidget *parent) override;

	QRectF label_rect(const QRectF &rect) const override;

	/**
	 * Paints the marker's label to the ruler.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the ruler client area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	void paint_label(QPainter &p, const QRect &rect, bool hover) override;

	/**
	 * Paints the background layer of the item with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_back(QPainter &p, ViewItemPaintParams &pp) override;

	/**
	 * Constructs the string to display.
	 */
	QString format_string(int max_width = 0, std::function<double(const QString&)> query_size
		= [](const QString& s) -> double { (void)s; return 0; });

	pair<float, float> get_cursor_offsets() const;

	virtual void on_setting_changed(const QString &key, const QVariant &value) override;

public Q_SLOTS:
	void on_hover_point_changed(const QWidget* widget, const QPoint &hp);

private:
	QString format_string_sub(int time_precision, int freq_precision, bool show_unit = true);

private:
	shared_ptr<Cursor> first_, second_;
	QColor fill_color_;

	QSizeF text_size_;
	QRectF label_area_;
	bool label_incomplete_;
	bool show_interval_, show_frequency_, show_samples_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_CURSORPAIR_HPP
