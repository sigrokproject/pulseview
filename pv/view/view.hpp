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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_VIEW_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_VIEW_HPP

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
#include <pv/views/viewbase.hpp>
#include <pv/util.hpp>

#include "cursorpair.hpp"
#include "flag.hpp"
#include "tracetreeitemowner.hpp"

namespace sigrok {
class ChannelGroup;
}

namespace pv {

class Session;

namespace views {

namespace TraceView {

class CursorHeader;
class DecodeTrace;
class Header;
class Ruler;
class Signal;
class Trace;
class Viewport;
class TriggerMarker;

class CustomAbstractScrollArea : public QAbstractScrollArea {
	Q_OBJECT

public:
	CustomAbstractScrollArea(QWidget *parent = 0);
	void setViewportMargins(int left, int top, int right, int bottom);
	bool viewportEvent(QEvent *event);
};

class View : public ViewBase, public TraceTreeItemOwner {
	Q_OBJECT

private:
	enum StickyEvents {
		TraceTreeItemHExtentsChanged = 1,
		TraceTreeItemVExtentsChanged = 2
	};

private:
	static const pv::util::Timestamp MaxScale;
	static const pv::util::Timestamp MinScale;

	static const int MaxScrollValue;
	static const int MaxViewAutoUpdateRate;

	static const int ScaleUnits[3];

public:
	explicit View(Session &session, bool is_main_view=false, QWidget *parent = 0);

	Session& session();
	const Session& session() const;

	/**
	 * Returns the signals contained in this view.
	 */
	std::unordered_set< std::shared_ptr<Signal> > signals() const;

	virtual void clear_signals();

	virtual void add_signal(const std::shared_ptr<Signal> signal);

#ifdef ENABLE_DECODE
	virtual void clear_decode_signals();

	virtual void add_decode_signal(std::shared_ptr<data::SignalBase> signalbase);

	virtual void remove_decode_signal(std::shared_ptr<data::SignalBase> signalbase);
#endif

	/**
	 * Returns the view of the owner.
	 */
	virtual View* view();

	/**
	 * Returns the view of the owner.
	 */
	virtual const View* view() const;

	Viewport* viewport();

	const Viewport* viewport() const;

	virtual void save_settings(QSettings &settings) const;

	virtual void restore_settings(QSettings &settings);

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
	const pv::util::Timestamp& offset() const;

	/**
	 * Returns the vertical scroll offset.
	 */
	int owner_visual_v_offset() const;

	/**
	 * Sets the visual v-offset.
	 */
	void set_v_offset(int offset);

	/**
	 * Returns the SI prefix to apply to the graticule time markings.
	 */
	pv::util::SIPrefix tick_prefix() const;

	/**
	 * Returns the number of fractional digits shown for the time markings.
	 */
	unsigned int tick_precision() const;

	/**
	 * Returns period of the graticule time markings.
	 */
	const pv::util::Timestamp& tick_period() const;

	/**
	 * Returns the unit of time currently used.
	 */
	util::TimeUnit time_unit() const;

	/**
	 * Returns the number of nested parents that this row item owner has.
	 */
	unsigned int depth() const;

	void zoom(double steps);
	void zoom(double steps, int offset);

	void zoom_fit(bool gui_state);

	void zoom_one_to_one();

	/**
	 * Sets the scale and offset.
	 * @param scale The new view scale in seconds per pixel.
	 * @param offset The view time offset in seconds.
	 */
	void set_scale_offset(double scale, const pv::util::Timestamp& offset);

	std::set< std::shared_ptr<pv::data::SignalData> >
		get_visible_data() const;

	std::pair<pv::util::Timestamp, pv::util::Timestamp> get_time_extents() const;

	/**
	 * Enables or disables coloured trace backgrounds. If they're not
	 * coloured then they will use alternating colors.
	 */
	void enable_coloured_bg(bool state);

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
	void add_flag(const pv::util::Timestamp& time);

	/**
	 * Removes a flag from the list.
	 */
	void remove_flag(std::shared_ptr<Flag> flag);

	/**
	 * Gets the list of flags.
	 */
	std::vector< std::shared_ptr<Flag> > flags() const;

	const QPoint& hover_point() const;

	void restack_all_trace_tree_items();

Q_SIGNALS:
	void hover_point_changed();

	void selection_changed();

	/// Emitted when the offset changed.
	void offset_changed();

	/// Emitted when the scale changed.
	void scale_changed();

	void sticky_scrolling_changed(bool state);

	void always_zoom_to_fit_changed(bool state);

	/// Emitted when the tick_prefix changed.
	void tick_prefix_changed();

	/// Emitted when the tick_precision changed.
	void tick_precision_changed();

	/// Emitted when the tick_period changed.
	void tick_period_changed();

	/// Emitted when the time_unit changed.
	void time_unit_changed();

public Q_SLOTS:
	void trigger_event(util::Timestamp location);

private:
	void get_scroll_layout(double &length, pv::util::Timestamp &offset) const;

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

	void reset_scroll();

	void set_scroll_default();

	void update_layout();

	TraceTreeItemOwner* find_prevalent_trace_group(
		const std::shared_ptr<sigrok::ChannelGroup> &group,
		const std::unordered_map<std::shared_ptr<data::SignalBase>,
			std::shared_ptr<Signal> > &signal_map);

	static std::vector< std::shared_ptr<Trace> >
		extract_new_traces_for_channels(
		const std::vector< std::shared_ptr<sigrok::Channel> > &channels,
		const std::unordered_map<std::shared_ptr<data::SignalBase>,
			std::shared_ptr<Signal> > &signal_map,
		std::set< std::shared_ptr<Trace> > &add_list);

	void determine_time_unit();

	bool eventFilter(QObject *object, QEvent *event);

	void resizeEvent(QResizeEvent *event);

public:
	void row_item_appearance_changed(bool label, bool content);
	void time_item_appearance_changed(bool label, bool content);

	void extents_changed(bool horz, bool vert);

private Q_SLOTS:

	void h_scroll_value_changed(int value);
	void v_scroll_value_changed();

	void signals_changed();
	void capture_state_updated(int state);
	void data_updated();

	void perform_delayed_view_update();

	void process_sticky_events();

	void on_hover_point_changed();

	/**
	 * Sets the 'offset_' member and emits the 'offset_changed'
	 * signal if needed.
	 */
	void set_offset(const pv::util::Timestamp& offset);

	/**
	 * Sets the 'scale_' member and emits the 'scale_changed'
	 * signal if needed.
	 */
	void set_scale(double scale);

	/**
	 * Sets the 'tick_prefix_' member and emits the 'tick_prefix_changed'
	 * signal if needed.
	 */
	void set_tick_prefix(pv::util::SIPrefix tick_prefix);

	/**
	 * Sets the 'tick_precision_' member and emits the 'tick_precision_changed'
	 * signal if needed.
	 */
	void set_tick_precision(unsigned tick_precision);

	/**
	 * Sets the 'tick_period_' member and emits the 'tick_period_changed'
	 * signal if needed.
	 */
	void set_tick_period(const pv::util::Timestamp& tick_period);

	/**
	 * Sets the 'time_unit' member and emits the 'time_unit_changed'
	 * signal if needed.
	 */
	void set_time_unit(pv::util::TimeUnit time_unit);

private:
	Viewport *viewport_;
	Ruler *ruler_;
	Header *header_;

	std::unordered_set< std::shared_ptr<Signal> > signals_;

#ifdef ENABLE_DECODE
	std::vector< std::shared_ptr<DecodeTrace> > decode_traces_;
#endif

	CustomAbstractScrollArea scrollarea_;

	/// The view time scale in seconds per pixel.
	double scale_;

	/// The view time offset in seconds.
	pv::util::Timestamp offset_;

	bool updating_scroll_;
	bool sticky_scrolling_;
	bool coloured_bg_;
	bool always_zoom_to_fit_;
	QTimer delayed_view_updater_;

	pv::util::Timestamp tick_period_;
	pv::util::SIPrefix tick_prefix_;
	unsigned int tick_precision_;
	util::TimeUnit time_unit_;

	bool show_cursors_;
	std::shared_ptr<CursorPair> cursors_;

	std::list< std::shared_ptr<Flag> > flags_;
	char next_flag_text_;

	std::vector< std::shared_ptr<TriggerMarker> > trigger_markers_;

	QPoint hover_point_;

	unsigned int sticky_events_;
	QTimer lazy_event_handler_;

	// This is true when the defaults couldn't be set due to insufficient info
	bool scroll_needs_defaults_;

	// A nonzero value indicates the v offset to restore. See View::resizeEvent()
	int saved_v_offset_;

	bool size_finalized_;
};

} // namespace TraceView
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_VIEW_HPP
