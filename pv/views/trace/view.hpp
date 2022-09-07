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

#ifndef PULSEVIEW_PV_VIEWS_TRACE_VIEW_HPP
#define PULSEVIEW_PV_VIEWS_TRACE_VIEW_HPP

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <QAbstractScrollArea>
#include <QShortcut>
#include <QSizeF>
#include <QSplitter>

#include <pv/globalsettings.hpp>
#include <pv/util.hpp>
#include <pv/data/signaldata.hpp>
#include <pv/views/viewbase.hpp>

#include "cursorpair.hpp"
#include "flag.hpp"
#include "trace.hpp"
#include "tracetreeitemowner.hpp"

using std::list;
using std::map;
using std::set;
using std::shared_ptr;
using std::vector;

namespace sigrok {
class ChannelGroup;
}

namespace pv {

class Session;

namespace data {
class Logic;
}

namespace views {

namespace trace {

class DecodeTrace;
class Header;
class Ruler;
class Signal;
class Viewport;
class TriggerMarker;

class CustomScrollArea : public QAbstractScrollArea
{
	Q_OBJECT

public:
	CustomScrollArea(QWidget *parent = nullptr);
	bool viewportEvent(QEvent *event);
};

// This Navigation code handles customisation of keyboard and mousewheel controls
// for navigating the traces by moving them and zooming them.
// 
// There are 5 sets of keys/mousewheels usable for customisation:
//   up/down, left/right, pageup/pagedown,
//   horizontal mousewheel and vertical mousewheel.
// There are 3 modifier keys that can be used with the above, as well as using
// no modifier. These are:
//   none, alt, ctrl, shift.
// Together the main and modifiers provide 5 * 4 = 20 different navigation controls
// for customisation.
// 
// There are 4 types of navigation that the above can be assigned to do:
//   none = no operation performed (same as disabling the operation)
//   zoom = zoom in/out of the trace view
//   hori = move the trace horizontally to see previous or next sample data
//   vert = move the trace vertically to see channels above or below
// 
// There is an "amount" variable of type "double" to be used with navigation.
// When moving traces horizontally or vertically the "amount" value is equal to
// the number of pages to move. So a value of 1.0 would move 1 page forward.
// A value of -0.5 would move half a page backwards.
// When zooming into traces the "amount" value is equal to the zoom multiplier.
// So a value of 1.0 will zoom out by 1x and a value of -2.0 will zoom in by 2x.
// 
// A "Navigation" page has been added to the Settings to allow the user to
// customise the navigation controls to their liking.

// type of navigation to perform
#define NAV_TYPE_NONE		0
#define NAV_TYPE_ZOOM		1
#define NAV_TYPE_HORI		2
#define NAV_TYPE_VERT		3

// navigation using keyboard
typedef struct {
	int type;			// NAV_TYPE_??? value
	double amount;		// amount to perform nav operation by
	QShortcut* sc;
} NavKb_t;

// navigation using mousewheel
typedef struct {
	int type;			// NAV_TYPE_??? value
	double amount;		// amount to perform nav operation by
} NavMw_t;

// keyboard
#define NAV_KB_VAR_DECLARE(name)	\
	NavKb_t name ## _nav_, name ## _alt_nav_, name ## _ctrl_nav_, name ## _shift_nav_

#define NAV_KB_VAR_SETUP(name, keys)			\
	name ## _nav_.type			= NAV_TYPE_NONE;\
	name ## _alt_nav_.type		= NAV_TYPE_NONE;\
	name ## _ctrl_nav_.type		= NAV_TYPE_NONE;\
	name ## _shift_nav_.type	= NAV_TYPE_NONE;\
	name ## _nav_.amount		= 0;			\
	name ## _alt_nav_.amount	= 0;			\
	name ## _ctrl_nav_.amount	= 0;			\
	name ## _shift_nav_.amount	= 0;			\
	name ## _nav_.sc		= new QShortcut(QKeySequence(keys),				this, SLOT(on_kb_ ##name ()),			nullptr, Qt::WidgetWithChildrenShortcut);	\
	name ## _alt_nav_.sc	= new QShortcut(QKeySequence(keys + Qt::ALT),	this, SLOT(on_kb_ ##name## _alt()),		nullptr, Qt::WidgetWithChildrenShortcut);	\
	name ## _ctrl_nav_.sc	= new QShortcut(QKeySequence(keys + Qt::CTRL),	this, SLOT(on_kb_ ##name## _ctrl()),	nullptr, Qt::WidgetWithChildrenShortcut);	\
	name ## _shift_nav_.sc	= new QShortcut(QKeySequence(keys + Qt::SHIFT),	this, SLOT(on_kb_ ##name## _shift()),	nullptr, Qt::WidgetWithChildrenShortcut)

#define NAV_KB_FUNC_DECLARE1(name)	\
	void on_kb_ ## name ()

#define NAV_KB_FUNC_DECLARE(name)			\
	NAV_KB_FUNC_DECLARE1(name);				\
	NAV_KB_FUNC_DECLARE1(name ## _alt);		\
	NAV_KB_FUNC_DECLARE1(name ## _ctrl);	\
	NAV_KB_FUNC_DECLARE1(name ## _shift)

#define NAV_KB_FUNC_DEFINE1(name)		\
	void View::on_kb_ ## name ()	{	\
		if (     name ## _nav_.type == NAV_TYPE_NONE )return;	\
		else if (name ## _nav_.type == NAV_TYPE_ZOOM )nav_zoom(name ## _nav_.amount);		\
		else if (name ## _nav_.type == NAV_TYPE_HORI )nav_move_hori(name ## _nav_.amount);	\
		else if (name ## _nav_.type == NAV_TYPE_VERT )nav_move_vert(name ## _nav_.amount);	\
	}

#define NAV_KB_FUNC_DEFINE(name)		\
	NAV_KB_FUNC_DEFINE1(name)			\
	NAV_KB_FUNC_DEFINE1(name ## _alt)	\
	NAV_KB_FUNC_DEFINE1(name ## _ctrl)	\
	NAV_KB_FUNC_DEFINE1(name ## _shift)

#define NAV_KB_SETTING_CHANGED1(name, k1, k2)	\
	if (key == GlobalSettings::Key_Nav_ ## name ## Type) {	\
		k1 ## _nav_.type = settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt();	\
		k2 ## _nav_.type = settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt();	\
	}	\
	if (key == GlobalSettings::Key_Nav_ ## name ## Amount) {	\
		k1 ## _nav_.amount = -settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toDouble();	\
		k2 ## _nav_.amount =  settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toDouble();	\
	}
	
#define NAV_KB_SETTING_CHANGED(name, k1, k2)		\
	NAV_KB_SETTING_CHANGED1(name,			k1, k2)	\
	NAV_KB_SETTING_CHANGED1(name ## Alt,	k1 ## _alt, k2 ## _alt)		\
	NAV_KB_SETTING_CHANGED1(name ## Ctrl,	k1 ## _ctrl, k2 ## _ctrl)	\
	NAV_KB_SETTING_CHANGED1(name ## Shift,	k1 ## _shift, k2 ## _shift)

#define NAV_KB_SETTING_LOAD1(name,	k1, k2)	\
	if (settings.contains(GlobalSettings::Key_Nav_ ## name ## Type)) {	\
		k1 ## _nav_.type = settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt();	\
		k2 ## _nav_.type = settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt();	\
	}	\
	if (settings.contains(GlobalSettings::Key_Nav_ ## name ## Amount)) {	\
		k1 ## _nav_.amount = -settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toDouble();	\
		k2 ## _nav_.amount =  settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toDouble();	\
	}

#define NAV_KB_SETTING_LOAD(name,	k1, k2)		\
	NAV_KB_SETTING_LOAD1(name,			k1, k2)	\
	NAV_KB_SETTING_LOAD1(name ## Alt,	k1 ## _alt, k2 ## _alt)		\
	NAV_KB_SETTING_LOAD1(name ## Ctrl,	k1 ## _ctrl, k2 ## _ctrl)	\
	NAV_KB_SETTING_LOAD1(name ## Shift,	k1 ## _shift, k2 ## _shift)

// mousewheel
#define NAV_MW_VAR_DECLARE(name)		\
	NavMw_t name ## _nav_, name ## _alt_nav_, name ## _ctrl_nav_, name ## _shift_nav_

#define NAV_MW_VAR_SETUP(name)			\
	name ## _nav_.type			= NAV_TYPE_NONE;\
	name ## _alt_nav_.type		= NAV_TYPE_NONE;\
	name ## _ctrl_nav_.type		= NAV_TYPE_NONE;\
	name ## _shift_nav_.type	= NAV_TYPE_NONE;\
	name ## _nav_.amount		= 0;			\
	name ## _alt_nav_.amount	= 0;			\
	name ## _ctrl_nav_.amount	= 0;			\
	name ## _shift_nav_.amount	= 0

#define NAV_MW_FUNC_DECLARE1(name)	\
	void on_mw_ ## name (QWheelEvent *event)

#define NAV_MW_FUNC_DECLARE(name)			\
	NAV_MW_FUNC_DECLARE1(name);				\
	NAV_MW_FUNC_DECLARE1(name ## _alt);		\
	NAV_MW_FUNC_DECLARE1(name ## _ctrl);	\
	NAV_MW_FUNC_DECLARE1(name ## _shift)

#define NAV_MW_FUNC_DEFINE1(name)		\
	void View::on_mw_ ## name (QWheelEvent *event)	{	\
		if (     name ## _nav_.type == NAV_TYPE_NONE )return;	\
		else if (name ## _nav_.type == NAV_TYPE_ZOOM )nav_zoom(     (-event->delta() * name ## _nav_.amount)/120.0, event->x());	\
		else if (name ## _nav_.type == NAV_TYPE_HORI )nav_move_hori((-event->delta() * name ## _nav_.amount)/120.0);	\
		else if (name ## _nav_.type == NAV_TYPE_VERT )nav_move_vert((-event->delta() * name ## _nav_.amount)/120.0);	\
	}

#define NAV_MW_FUNC_DEFINE(name)		\
	NAV_MW_FUNC_DEFINE1(name)			\
	NAV_MW_FUNC_DEFINE1(name ## _alt)	\
	NAV_MW_FUNC_DEFINE1(name ## _ctrl)	\
	NAV_MW_FUNC_DEFINE1(name ## _shift)

#define NAV_MW_SETTING_CHANGED1(name, mw)	\
	if (key == GlobalSettings::Key_Nav_ ## name ## Type) {	\
		mw ## _nav_.type = settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt();	\
	}	\
	if (key == GlobalSettings::Key_Nav_ ## name ## Amount) {	\
		mw ## _nav_.amount = settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toDouble();	\
	}
	
#define NAV_MW_SETTING_CHANGED(name, mw)		\
	NAV_MW_SETTING_CHANGED1(name,			mw)	\
	NAV_MW_SETTING_CHANGED1(name ## Alt,	mw ## _alt)		\
	NAV_MW_SETTING_CHANGED1(name ## Ctrl,	mw ## _ctrl)	\
	NAV_MW_SETTING_CHANGED1(name ## Shift,	mw ## _shift)

#define NAV_MW_SETTING_LOAD1(name,	mw)		\
	if (settings.contains(GlobalSettings::Key_Nav_ ## name ## Type)) {	\
		mw ## _nav_.type = settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt();	\
	}	\
	if (settings.contains(GlobalSettings::Key_Nav_ ## name ## Amount)) {	\
		mw ## _nav_.amount = settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toDouble();	\
	}

#define NAV_MW_SETTING_LOAD(name,	mw)		\
	NAV_MW_SETTING_LOAD1(name,			mw)	\
	NAV_MW_SETTING_LOAD1(name ## Alt,	mw ## _alt)		\
	NAV_MW_SETTING_LOAD1(name ## Ctrl,	mw ## _ctrl)	\
	NAV_MW_SETTING_LOAD1(name ## Shift,	mw ## _shift)


class View : public ViewBase, public TraceTreeItemOwner, public GlobalSettingsInterface
{
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
	static const int ViewScrollMargin;

	static const int ScaleUnits[3];

public:
	explicit View(Session &session, bool is_main_view=false, QMainWindow *parent = nullptr);

	~View();

	virtual ViewType get_type() const;

	/**
	 * Resets the view to its default state after construction. It does however
	 * not reset the signal bases or any other connections with the session.
	 */
	virtual void reset_view_state();

	Session& session();  // This method is needed for TraceTreeItemOwner, not ViewBase
	const Session& session() const;  // This method is needed for TraceTreeItemOwner, not ViewBase

	/**
	 * Returns the signals contained in this view.
	 */
	vector< shared_ptr<Signal> > signals() const;

	shared_ptr<Signal> get_signal_by_signalbase(shared_ptr<data::SignalBase> base) const;

	virtual void clear_signalbases();
	virtual void add_signalbase(const shared_ptr<data::SignalBase> signalbase);
	virtual void remove_signalbase(const shared_ptr<data::SignalBase> signalbase);

#ifdef ENABLE_DECODE
	virtual void clear_decode_signals();

	virtual void add_decode_signal(shared_ptr<data::DecodeSignal> signal);

	virtual void remove_decode_signal(shared_ptr<data::DecodeSignal> signal);
#endif

	void remove_trace(shared_ptr<Trace> trace);

	shared_ptr<Signal> get_signal_under_mouse_cursor() const;

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

	QAbstractScrollArea* scrollarea() const;

	const Ruler* ruler() const;

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

	/**
	 * Gets a list of time markers.
	 */
	vector< shared_ptr<TimeItem> > time_items() const;

	/**
	 * Returns the view time scale in seconds per pixel.
	 */
	double scale() const;

	/**
	 * Returns the internal view version of the time offset of the left edge
	 * of the view in seconds.
	 */
	const pv::util::Timestamp& offset() const;

	/**
	 * Returns the ruler version of the time offset of the left edge
	 * of the view in seconds.
	 */
	const pv::util::Timestamp& ruler_offset() const;

	void set_zero_position(const pv::util::Timestamp& position);

	void reset_zero_position();

	pv::util::Timestamp zero_offset() const;

	/**
	 * Returns the vertical scroll offset.
	 */
	int owner_visual_v_offset() const;

	/**
	 * Sets the visual v-offset.
	 */
	void set_v_offset(int offset);

	/**
	 * Sets the visual h-offset.
	 */
	void set_h_offset(int offset);

	/**
	 * Gets the length of the horizontal scrollbar.
	 */
	int get_h_scrollbar_maximum() const;

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
	 * Returns number of minor division ticks per time marking.
	 */
	unsigned int minor_tick_count() const;

	/**
	 * Returns the unit of time currently used.
	 */
	util::TimeUnit time_unit() const;

	/**
	 * Returns the number of nested parents that this row item owner has.
	 */
	unsigned int depth() const;

	/**
	 * Returns the currently displayed segment, starting at 0.
	 */
	uint32_t current_segment() const;

	/**
	 * Returns whether the currently shown segment can be influenced
	 * (selected) or not.
	 */
	bool segment_is_selectable() const;

	Trace::SegmentDisplayMode segment_display_mode() const;
	void set_segment_display_mode(Trace::SegmentDisplayMode mode);

	void zoom(double steps);
	void zoom(double steps, int offset);

	void zoom_fit(bool gui_state);

	virtual void focus_on_range(uint64_t start_sample, uint64_t end_sample);

	/**
	 * Sets the scale and offset.
	 * @param scale The new view scale in seconds per pixel.
	 * @param offset The view time offset in seconds.
	 */
	void set_scale_offset(double scale, const pv::util::Timestamp& offset);

	vector< shared_ptr<pv::data::SignalData> > get_visible_data() const;

	pair<pv::util::Timestamp, pv::util::Timestamp> get_time_extents() const;

	/**
	 * Returns true if the trace background should be drawn with a colored background.
	 */
	bool colored_bg() const;

	/**
	 * Returns true if cursors are displayed, false otherwise.
	 */
	bool cursors_shown() const;

	/**
	 * Shows or hides the cursors.
	 */
	void show_cursors(bool show = true);

	/**
	 * Sets the cursors to the given offsets.
	 * You still have to call show_cursors() separately.
	 */
	void set_cursors(pv::util::Timestamp& first, pv::util::Timestamp& second);

	/**
	 * Moves the cursors to a convenient position in the view.
	 * You still have to call show_cursors() separately.
	 */
	void center_cursors();

	/**
	 * Returns a reference to the pair of cursors.
	 */
	shared_ptr<CursorPair> cursors() const;

	/**
	 * Adds a new flag at a specified time.
	 */
	shared_ptr<Flag> add_flag(const pv::util::Timestamp& time);

	/**
	 * Removes a flag from the list.
	 */
	void remove_flag(shared_ptr<Flag> flag);

	/**
	 * Gets the list of flags.
	 */
	vector< shared_ptr<Flag> > flags() const;

	const QPoint& hover_point() const;
	const QWidget* hover_widget() const;

	/**
	 * Determines the closest level change (i.e. edge) to a given point, which
	 * is useful for e.g. the "snap to edge" functionality.
	 *
	 * @param p The current position of the mouse cursor
	 * @return The sample number of the nearest level change or -1 if none
	 */
	int64_t get_nearest_level_change(const QPoint &p);

	void restack_all_trace_tree_items();

	int header_width() const;

	void on_setting_changed(const QString &key, const QVariant &value);

Q_SIGNALS:
	void hover_point_changed(const QWidget* widget, const QPoint &hp);

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

	/// Emitted when the currently selected segment changed
	void segment_changed(int segment_id);

	/// Emitted when the multi-segment display mode changed
	/// @param mode is a value of Trace::SegmentDisplayMode
	void segment_display_mode_changed(int mode, bool segment_selectable);

	/// Emitted when the cursors are shown/hidden
	void cursor_state_changed(bool show);

public Q_SLOTS:
	void trigger_event(int segment_id, util::Timestamp location);

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

	void adjust_top_margin();

	void update_scroll();
	void reset_scroll();
	void set_scroll_default();

	void determine_if_header_was_shrunk();

	void resize_header_to_fit();

	void update_layout();

	TraceTreeItemOwner* find_prevalent_trace_group(
		const shared_ptr<sigrok::ChannelGroup> &group,
		const map<shared_ptr<data::SignalBase>, shared_ptr<Signal> > &signal_map);

	static vector< shared_ptr<Trace> >
		extract_new_traces_for_channels(
		const vector< shared_ptr<sigrok::Channel> > &channels,
		const map<shared_ptr<data::SignalBase>, shared_ptr<Signal> > &signal_map,
		set< shared_ptr<Trace> > &add_list);

	void determine_time_unit();

	bool eventFilter(QObject *object, QEvent *event);

	virtual void contextMenuEvent(QContextMenuEvent *event);

	void resizeEvent(QResizeEvent *event);

	void update_view_range_metaobject() const;
	void update_hover_point();

public:
	void row_item_appearance_changed(bool label, bool content);
	void time_item_appearance_changed(bool label, bool content);

	void extents_changed(bool horz, bool vert);

	void nav_zoom(double amount);
	void nav_zoom(double amount, int offset);
	void nav_move_hori(double amount);
	void nav_move_vert(double amount);
	
	void on_mw_vert_all(QWheelEvent *event);
	void on_mw_hori_all(QWheelEvent *event);
	
private Q_SLOTS:
	void on_signal_name_changed();
	void on_splitter_moved();

	void on_zoom_in_shortcut_triggered();
	void on_zoom_out_shortcut_triggered();
	void on_scroll_to_start_shortcut_triggered();
	void on_scroll_to_end_shortcut_triggered();

	void h_scroll_value_changed(int value);
	void v_scroll_value_changed();

	void on_grab_ruler(int ruler_id);

	void signals_changed();
	void capture_state_updated(int state);

	// keyboard and mousewheel navigation functions
	NAV_KB_FUNC_DECLARE(up);
	NAV_KB_FUNC_DECLARE(down);
	NAV_KB_FUNC_DECLARE(left);
	NAV_KB_FUNC_DECLARE(right);
	NAV_KB_FUNC_DECLARE(pageup);
	NAV_KB_FUNC_DECLARE(pagedown);
	NAV_MW_FUNC_DECLARE(hori);
	NAV_MW_FUNC_DECLARE(vert);
	
	void on_new_segment(int new_segment_id);
	void on_segment_completed(int new_segment_id);
	void on_segment_changed(int segment);

	void on_settingViewTriggerIsZeroTime_changed(const QVariant new_value);

	void on_create_marker_here();

	virtual void perform_delayed_view_update();

	void process_sticky_events();

	/**
	 * Sets the 'offset_' and ruler_offset_ members and emits the 'offset_changed'
	 * signal if needed.
	 */
	void set_offset(const pv::util::Timestamp& offset, bool force_update = false);

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

	/**
	 * Sets the current segment with the first segment starting at 0.
	 */
	void set_current_segment(uint32_t segment_id);

private:
	CustomScrollArea *scrollarea_;
	Viewport *viewport_;
	Ruler *ruler_;
	Header *header_;
	QSplitter *splitter_;

	QShortcut *zoom_in_shortcut_, *zoom_out_shortcut_;
	QShortcut *home_shortcut_, *end_shortcut_;
	QShortcut *grab_ruler_left_shortcut_, *grab_ruler_right_shortcut_;
	QShortcut *cancel_grab_shortcut_;
	
	// keyboard and mousewheel navigation variables
	NAV_KB_VAR_DECLARE(up);
	NAV_KB_VAR_DECLARE(down);
	NAV_KB_VAR_DECLARE(left);
	NAV_KB_VAR_DECLARE(right);
	NAV_KB_VAR_DECLARE(pageup);
	NAV_KB_VAR_DECLARE(pagedown);
	NAV_MW_VAR_DECLARE(hori);
	NAV_MW_VAR_DECLARE(vert);
	
	mutable mutex signal_mutex_;
	vector< shared_ptr<Signal> > signals_;

#ifdef ENABLE_DECODE
	vector< shared_ptr<DecodeTrace> > decode_traces_;
#endif

	Trace::SegmentDisplayMode segment_display_mode_;

	/// Signals whether the user can change the currently shown segment.
	bool segment_selectable_;

	/// The view time scale in seconds per pixel.
	double scale_;

	/// The internal view version of the time offset in seconds.
	pv::util::Timestamp offset_;
	/// The ruler version of the time offset in seconds.
	pv::util::Timestamp ruler_offset_;
	/// The offset of the zero point in seconds.
	pv::util::Timestamp zero_offset_;
	/// Shows whether the user set a custom zero offset that we should keep
	bool custom_zero_offset_set_;

	bool updating_scroll_;
	bool restoring_state_;
	bool header_was_shrunk_;

	bool sticky_scrolling_;
	bool colored_bg_;
	bool always_zoom_to_fit_;

	pv::util::Timestamp tick_period_;
	pv::util::SIPrefix tick_prefix_;
	unsigned int minor_tick_count_;
	unsigned int tick_precision_;
	util::TimeUnit time_unit_;

	bool show_cursors_;
	shared_ptr<CursorPair> cursors_;

	list< shared_ptr<Flag> > flags_;
	char next_flag_text_;

	vector< shared_ptr<TriggerMarker> > trigger_markers_;

	QWidget* hover_widget_;
	TimeMarker* grabbed_widget_;
	QPoint hover_point_;
	shared_ptr<Signal> signal_under_mouse_cursor_;
	uint16_t snap_distance_;

	unsigned int sticky_events_;
	QTimer lazy_event_handler_;

	// This is true when the defaults couldn't be set due to insufficient info
	bool scroll_needs_defaults_;

	// The v offset to restore. See View::eventFilter()
	int saved_v_offset_;

	// These are used to determine whether the view was altered after acq started
	double scale_at_acq_start_;
	pv::util::Timestamp offset_at_acq_start_;

	// X coordinate of mouse cursor where the user clicked to open a context menu
	uint32_t context_menu_x_pos_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_VIEW_HPP
