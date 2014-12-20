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

#ifndef PULSEVIEW_PV_VIEW_VIEW_H
#define PULSEVIEW_PV_VIEW_VIEW_H

#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <QAbstractScrollArea>
#include <QSizeF>
#include <QTimer>

#include <pv/data/signaldata.hpp>

#include "cursorpair.hpp"
#include "flag.hpp"
#include "rowitemowner.hpp"

namespace pv {

class Session;

namespace view {

class CursorHeader;
class Header;
class Ruler;
class Viewport;

class View : public QAbstractScrollArea, public RowItemOwner {
	Q_OBJECT

private:
	enum StickyEvents {
		RowItemHExtentsChanged = 1,
		RowItemVExtentsChanged = 2
	};

private:
	static const double MaxScale;
	static const double MinScale;

	static const int MaxScrollValue;

	static const int ScaleUnits[3];

public:
	static const QSizeF LabelPadding;

public:
	explicit View(Session &session, QWidget *parent = 0);

	Session& session();
	const Session& session() const;

	/**
	 * Returns the view of the owner.
	 */
	virtual pv::view::View* view();

	/**
	 * Returns the view of the owner.
	 */
	virtual const pv::view::View* view() const;

	Viewport* viewport();

	const Viewport* viewport() const;

	/**
	 * Gets a list of time markers.
	 */
	std::vector< std::shared_ptr<TimeItem> > time_items() const;

	/**
	 * Returns the view time scale in seconds per pixel.
	 */
	double scale() const;

	/**
	 * Returns the time offset of the left edge of the view in
	 * seconds.
	 */
	double offset() const;
	int owner_visual_v_offset() const;

	/**
	 * Returns the SI prefix to apply to the graticule time markings.
	 */
	unsigned int tick_prefix() const;

	/**
	 * Returns period of the graticule time markings.
	 */
	double tick_period() const;

	/**
	 * Returns the number of nested parents that this row item owner has.
	 */
	unsigned int depth() const;

	void zoom(double steps);
	void zoom(double steps, int offset);

	void zoom_fit();

	void zoom_one_to_one();

	/**
	 * Sets the scale and offset.
	 * @param scale The new view scale in seconds per pixel.
	 * @param offset The view time offset in seconds.
	 */
	void set_scale_offset(double scale, double offset);

	std::set< std::shared_ptr<pv::data::SignalData> >
		get_visible_data() const;

	std::pair<double, double> get_time_extents() const;

	/**
	 * Returns true if cursors are displayed. false otherwise.
	 */
	bool cursors_shown() const;

	/**
	 * Shows or hides the cursors.
	 */
	void show_cursors(bool show = true);

	/**
	 * Moves the cursors to a convenient position in the view.
	 */
	void centre_cursors();

	/**
	 * Returns a reference to the pair of cursors.
	 */
	std::shared_ptr<CursorPair> cursors() const;

	/**
	 * Adds a new flag at a specified time.
	 */
	void add_flag(double time);

	/**
	 * Removes a flag from the list.
	 */
	void remove_flag(std::shared_ptr<Flag> flag);

	/**
	 * Gets the list of flags.
	 */
	std::vector< std::shared_ptr<Flag> > flags() const;

	const QPoint& hover_point() const;

	void update_viewport();

	void restack_all_row_items();

Q_SIGNALS:
	void hover_point_changed();

	void signals_moved();

	void selection_changed();

	void scale_offset_changed();

private:
	void get_scroll_layout(double &length, double &offset) const;

	/**
	 * Simultaneously sets the zoom and offset.
	 * @param scale The scale to set the view to in seconds per pixel. This
	 * value is clamped between MinScale and MaxScale.
	 * @param offset The offset of the left edge of the view in seconds.
	 */
	void set_zoom(double scale, int offset);

	/**
	 * Find a tick spacing and number formatting that does not cause
	 * the values to collide.
	 */
	void calculate_tick_spacing();

	void update_scroll();

	void update_layout();

	/**
	 * Satisifies RowItem functionality.
	 * @param p the QPainter to paint into.
	 * @param rect the rectangle of the header area.
	 * @param hover true if the label is being hovered over by the mouse.
	 */
	void paint_label(QPainter &p, const QRect &rect, bool hover);

	/**
	 * Computes the outline rectangle of a label.
	 * @param rect the rectangle of the header area.
	 * @return Returns the rectangle of the signal label.
	 */
	QRectF label_rect(const QRectF &rect);

	static bool add_channels_to_owner(
		const std::vector< std::shared_ptr<sigrok::Channel> > &channels,
		RowItemOwner *owner, int &offset,
		std::unordered_map<std::shared_ptr<sigrok::Channel>,
			std::shared_ptr<Signal> > &signal_map,
		std::function<bool (std::shared_ptr<RowItem>)> filter_func =
			std::function<bool (std::shared_ptr<RowItem>)>());

	static void apply_offset(
		std::shared_ptr<RowItem> row_item, int &offset);

private:
	bool eventFilter(QObject *object, QEvent *event);

	bool viewportEvent(QEvent *e);

	void resizeEvent(QResizeEvent *e);

public:
	void row_item_appearance_changed(bool label, bool content);
	void time_item_appearance_changed(bool label, bool content);

	void extents_changed(bool horz, bool vert);

private Q_SLOTS:

	void h_scroll_value_changed(int value);
	void v_scroll_value_changed(int value);

	void signals_changed();
	void data_updated();

	void on_signals_moved();

	void process_sticky_events();

	void on_hover_point_changed();

private:
	Session &session_;

	Viewport *viewport_;
	Ruler *ruler_;
	Header *header_;

	/// The view time scale in seconds per pixel.
	double scale_;

	/// The view time offset in seconds.
	double offset_;

	int v_offset_;
	bool updating_scroll_;

	double tick_period_;
	unsigned int tick_prefix_;

	bool show_cursors_;
	std::shared_ptr<CursorPair> cursors_;

	std::list< std::shared_ptr<Flag> > flags_;
	char next_flag_text_;

	QPoint hover_point_;

	unsigned int sticky_events_;
	QTimer lazy_event_handler_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_VIEW_H
