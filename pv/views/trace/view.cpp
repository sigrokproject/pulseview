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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <extdef.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <iostream>
#include <iterator>
#include <unordered_set>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/serialization.hpp>

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "analogsignal.hpp"
#include "header.hpp"
#include "logicsignal.hpp"
#include "ruler.hpp"
#include "signal.hpp"
#include "tracegroup.hpp"
#include "triggermarker.hpp"
#include "view.hpp"
#include "viewport.hpp"

#include "pv/data/logic.hpp"
#include "pv/data/logicsegment.hpp"
#include "pv/devices/device.hpp"
#include "pv/globalsettings.hpp"
#include "pv/session.hpp"
#include "pv/util.hpp"

#ifdef ENABLE_DECODE
#include "decodetrace.hpp"
#endif

using pv::data::SignalData;
using pv::data::Segment;
using pv::util::TimeUnit;
using pv::util::Timestamp;

using std::back_inserter;
using std::copy_if;
using std::count_if;
using std::inserter;
using std::max;
using std::make_pair;
using std::make_shared;
using std::min;
using std::pair;
using std::set;
using std::set_difference;
using std::shared_ptr;
using std::stringstream;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace pv {
namespace views {
namespace trace {

const Timestamp View::MaxScale("1e9");
const Timestamp View::MinScale("1e-12");

const int View::MaxScrollValue = INT_MAX / 2;

const int View::ScaleUnits[3] = {1, 2, 5};


CustomScrollArea::CustomScrollArea(QWidget *parent) :
	QAbstractScrollArea(parent)
{
}

bool CustomScrollArea::viewportEvent(QEvent *event)
{
	switch (event->type()) {
	case QEvent::Paint:
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
	case QEvent::MouseMove:
	case QEvent::Wheel:
	case QEvent::TouchBegin:
	case QEvent::TouchUpdate:
	case QEvent::TouchEnd:
		return false;
	default:
		return QAbstractScrollArea::viewportEvent(event);
	}
}

View::View(Session &session, bool is_main_view, QWidget *parent) :
	ViewBase(session, is_main_view, parent),

	// Note: Place defaults in View::reset_view_state(), not here
	splitter_(new QSplitter()),
	header_was_shrunk_(false),  // The splitter remains unchanged after a reset, so this goes here
	sticky_scrolling_(false)  // Default setting is set in MainWindow::setup_ui()
{
	QVBoxLayout *root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);
	root_layout->addWidget(splitter_);

	viewport_ = new Viewport(*this);
	scrollarea_ = new CustomScrollArea(splitter_);
	scrollarea_->setViewport(viewport_);
	scrollarea_->setFrameShape(QFrame::NoFrame);

	ruler_ = new Ruler(*this);

	header_ = new Header(*this);
	header_->setMinimumWidth(10);  // So that the arrow tips show at least

	// We put the header into a simple layout so that we can add the top margin,
	// allowing us to make it line up with the bottom of the ruler
	QWidget *header_container = new QWidget();
	header_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QVBoxLayout *header_layout = new QVBoxLayout(header_container);
	header_layout->setContentsMargins(0, ruler_->sizeHint().height(), 0, 0);
	header_layout->addWidget(header_);

	// To let the ruler and scrollarea be on the same split pane, we need a layout
	QWidget *trace_container = new QWidget();
	trace_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QVBoxLayout *trace_layout = new QVBoxLayout(trace_container);
	trace_layout->setSpacing(0);  // We don't want space between the ruler and scrollarea
	trace_layout->setContentsMargins(0, 0, 0, 0);
	trace_layout->addWidget(ruler_);
	trace_layout->addWidget(scrollarea_);

	splitter_->addWidget(header_container);
	splitter_->addWidget(trace_container);
	splitter_->setHandleWidth(1);  // Don't show a visible rubber band
	splitter_->setCollapsible(0, false);  // Prevent the header from collapsing
	splitter_->setCollapsible(1, false);  // Prevent the traces from collapsing
	splitter_->setStretchFactor(0, 0);  // Prevent the panes from being resized
	splitter_->setStretchFactor(1, 1);  // when the entire view is resized
	splitter_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	viewport_->installEventFilter(this);
	ruler_->installEventFilter(this);
	header_->installEventFilter(this);

	// Set up settings and event handlers
	GlobalSettings settings;
	colored_bg_ = settings.value(GlobalSettings::Key_View_ColoredBG).toBool();
	snap_distance_ = settings.value(GlobalSettings::Key_View_SnapDistance).toInt();

	GlobalSettings::add_change_handler(this);

	connect(scrollarea_->horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(scrollarea_->verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed()));

	connect(header_, SIGNAL(selection_changed()),
		ruler_, SLOT(clear_selection()));
	connect(ruler_, SIGNAL(selection_changed()),
		header_, SLOT(clear_selection()));

	connect(header_, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));
	connect(ruler_, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));

	connect(splitter_, SIGNAL(splitterMoved(int, int)),
		this, SLOT(on_splitter_moved()));

	connect(&lazy_event_handler_, SIGNAL(timeout()),
		this, SLOT(process_sticky_events()));
	lazy_event_handler_.setSingleShot(true);

	// Trigger the initial event manually. The default device has signals
	// which were created before this object came into being
	signals_changed();

	// make sure the transparent widgets are on the top
	ruler_->raise();
	header_->raise();

	reset_view_state();
}

View::~View()
{
	GlobalSettings::remove_change_handler(this);
}

void View::reset_view_state()
{
	ViewBase::reset_view_state();

	segment_display_mode_ = Trace::ShowLastSegmentOnly;
	segment_selectable_ = false;
	scale_ = 1e-3;
	offset_ = 0;
	ruler_offset_ = 0;
	updating_scroll_ = false;
	settings_restored_ = false;
	always_zoom_to_fit_ = false;
	tick_period_ = 0;
	tick_prefix_ = pv::util::SIPrefix::yocto;
	tick_precision_ = 0;
	time_unit_ = util::TimeUnit::Time;
	show_cursors_ = false;
	cursors_ = make_shared<CursorPair>(*this);
	next_flag_text_ = 'A';
	trigger_markers_.clear();
	hover_point_ = QPoint(-1, -1);
	scroll_needs_defaults_ = true;
	saved_v_offset_ = 0;
	scale_at_acq_start_ = 0;
	offset_at_acq_start_ = 0;
	suppress_zoom_to_fit_after_acq_ = false;

	show_cursors_ = false;
	cursor_state_changed(show_cursors_);
	flags_.clear();

	// Update the zoom state
	calculate_tick_spacing();

	// Make sure the standard bar's segment selector is in sync
	set_segment_display_mode(segment_display_mode_);
}

Session& View::session()
{
	return session_;
}

const Session& View::session() const
{
	return session_;
}

unordered_set< shared_ptr<Signal> > View::signals() const
{
	return signals_;
}

void View::clear_signals()
{
	ViewBase::clear_signalbases();
	signals_.clear();
}

void View::add_signal(const shared_ptr<Signal> signal)
{
	ViewBase::add_signalbase(signal->base());
	signals_.insert(signal);

	signal->set_segment_display_mode(segment_display_mode_);
	signal->set_current_segment(current_segment_);

	connect(signal->base().get(), SIGNAL(name_changed(const QString&)),
		this, SLOT(on_signal_name_changed()));
}

#ifdef ENABLE_DECODE
void View::clear_decode_signals()
{
	decode_traces_.clear();
}

void View::add_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	shared_ptr<DecodeTrace> d(
		new DecodeTrace(session_, signal, decode_traces_.size()));
	decode_traces_.push_back(d);

	d->set_segment_display_mode(segment_display_mode_);
	d->set_current_segment(current_segment_);

	connect(signal.get(), SIGNAL(name_changed(const QString&)),
		this, SLOT(on_signal_name_changed()));
}

void View::remove_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	for (auto i = decode_traces_.begin(); i != decode_traces_.end(); i++)
		if ((*i)->base() == signal) {
			decode_traces_.erase(i);
			signals_changed();
			return;
		}
}
#endif

shared_ptr<Signal> View::get_signal_under_mouse_cursor() const
{
	return signal_under_mouse_cursor_;
}

View* View::view()
{
	return this;
}

const View* View::view() const
{
	return this;
}

Viewport* View::viewport()
{
	return viewport_;
}

const Viewport* View::viewport() const
{
	return viewport_;
}

void View::save_settings(QSettings &settings) const
{
	settings.setValue("scale", scale_);
	settings.setValue("v_offset",
		scrollarea_->verticalScrollBar()->sliderPosition());

	settings.setValue("splitter_state", splitter_->saveState());
	settings.setValue("segment_display_mode", segment_display_mode_);

	{
		stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << boost::serialization::make_nvp("ruler_shift", ruler_shift_);
		settings.setValue("ruler_shift", QString::fromStdString(ss.str()));
	}
	{
		stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << boost::serialization::make_nvp("offset", offset_);
		settings.setValue("offset", QString::fromStdString(ss.str()));
	}

	for (shared_ptr<Signal> signal : signals_) {
		settings.beginGroup(signal->base()->internal_name());
		signal->save_settings(settings);
		settings.endGroup();
	}
}

void View::restore_settings(QSettings &settings)
{
	// Note: It is assumed that this function is only called once,
	// immediately after restoring a previous session.

	if (settings.contains("scale"))
		set_scale(settings.value("scale").toDouble());

	if (settings.contains("ruler_shift")) {
		util::Timestamp shift;
		stringstream ss;
		ss << settings.value("ruler_shift").toString().toStdString();

		try {
			boost::archive::text_iarchive ia(ss);
			ia >> boost::serialization::make_nvp("ruler_shift", shift);
			ruler_shift_ = shift;
		} catch (boost::archive::archive_exception&) {
			qDebug() << "Could not restore the view ruler shift";
		}
	}

	if (settings.contains("offset")) {
		util::Timestamp offset;
		stringstream ss;
		ss << settings.value("offset").toString().toStdString();

		try {
			boost::archive::text_iarchive ia(ss);
			ia >> boost::serialization::make_nvp("offset", offset);
			// This also updates ruler_offset_
			set_offset(offset);
		} catch (boost::archive::archive_exception&) {
			qDebug() << "Could not restore the view offset";
		}
	}

	if (settings.contains("splitter_state"))
		splitter_->restoreState(settings.value("splitter_state").toByteArray());

	if (settings.contains("segment_display_mode"))
		set_segment_display_mode(
			(Trace::SegmentDisplayMode)(settings.value("segment_display_mode").toInt()));

	for (shared_ptr<Signal> signal : signals_) {
		settings.beginGroup(signal->base()->internal_name());
		signal->restore_settings(settings);
		settings.endGroup();
	}

	if (settings.contains("v_offset")) {
		saved_v_offset_ = settings.value("v_offset").toInt();
		set_v_offset(saved_v_offset_);
		scroll_needs_defaults_ = false;
		// Note: see eventFilter() for additional information
	}

	settings_restored_ = true;
	suppress_zoom_to_fit_after_acq_ = true;

	// Update the ruler so that it uses the new scale
	calculate_tick_spacing();
}

vector< shared_ptr<TimeItem> > View::time_items() const
{
	const vector<shared_ptr<Flag>> f(flags());
	vector<shared_ptr<TimeItem>> items(f.begin(), f.end());

	if (cursors_) {
		items.push_back(cursors_);
		items.push_back(cursors_->first());
		items.push_back(cursors_->second());
	}

	for (auto trigger_marker : trigger_markers_)
		items.push_back(trigger_marker);

	return items;
}

double View::scale() const
{
	return scale_;
}

void View::set_scale(double scale)
{
	if (scale_ != scale) {
		scale_ = scale;
		scale_changed();
	}
}

void View::set_offset(const pv::util::Timestamp& offset, bool force_update)
{
	if ((offset_ != offset) || force_update) {
		offset_ = offset;
		ruler_offset_ = offset_ + ruler_shift_;
		offset_changed();
	}
}

const Timestamp& View::offset() const
{
	return offset_;
}

const Timestamp& View::ruler_offset() const
{
	return ruler_offset_;
}

void View::set_zero_position(pv::util::Timestamp& position)
{
	ruler_shift_ = -position;

	// Force an immediate update of the offsets
	set_offset(offset_, true);
	ruler_->update();
}

void View::reset_zero_position()
{
	ruler_shift_ = 0;

	// Force an immediate update of the offsets
	set_offset(offset_, true);
	ruler_->update();
}

int View::owner_visual_v_offset() const
{
	return -scrollarea_->verticalScrollBar()->sliderPosition();
}

void View::set_v_offset(int offset)
{
	scrollarea_->verticalScrollBar()->setSliderPosition(offset);
	header_->update();
	viewport_->update();
}

unsigned int View::depth() const
{
	return 0;
}

uint32_t View::current_segment() const
{
	return current_segment_;
}

pv::util::SIPrefix View::tick_prefix() const
{
	return tick_prefix_;
}

void View::set_tick_prefix(pv::util::SIPrefix tick_prefix)
{
	if (tick_prefix_ != tick_prefix) {
		tick_prefix_ = tick_prefix;
		tick_prefix_changed();
	}
}

unsigned int View::tick_precision() const
{
	return tick_precision_;
}

void View::set_tick_precision(unsigned tick_precision)
{
	if (tick_precision_ != tick_precision) {
		tick_precision_ = tick_precision;
		tick_precision_changed();
	}
}

const pv::util::Timestamp& View::tick_period() const
{
	return tick_period_;
}

unsigned int View::minor_tick_count() const
{
	return minor_tick_count_;
}

void View::set_tick_period(const pv::util::Timestamp& tick_period)
{
	if (tick_period_ != tick_period) {
		tick_period_ = tick_period;
		tick_period_changed();
	}
}

TimeUnit View::time_unit() const
{
	return time_unit_;
}

void View::set_time_unit(pv::util::TimeUnit time_unit)
{
	if (time_unit_ != time_unit) {
		time_unit_ = time_unit;
		time_unit_changed();
	}
}

void View::set_current_segment(uint32_t segment_id)
{
	current_segment_ = segment_id;

	for (shared_ptr<Signal> signal : signals_)
		signal->set_current_segment(current_segment_);
#ifdef ENABLE_DECODE
	for (shared_ptr<DecodeTrace> dt : decode_traces_)
		dt->set_current_segment(current_segment_);
#endif

	vector<util::Timestamp> triggers = session_.get_triggers(current_segment_);

	trigger_markers_.clear();
	for (util::Timestamp timestamp : triggers)
		trigger_markers_.push_back(make_shared<TriggerMarker>(*this, timestamp));

	// When enabled, the first trigger for this segment is used as the zero position
	GlobalSettings settings;
	bool trigger_is_zero_time = settings.value(GlobalSettings::Key_View_TriggerIsZeroTime).toBool();

	if (trigger_is_zero_time && (triggers.size() > 0))
		set_zero_position(triggers.front());

	viewport_->update();

	segment_changed(segment_id);
}

bool View::segment_is_selectable() const
{
	return segment_selectable_;
}

Trace::SegmentDisplayMode View::segment_display_mode() const
{
	return segment_display_mode_;
}

void View::set_segment_display_mode(Trace::SegmentDisplayMode mode)
{
	segment_display_mode_ = mode;

	for (shared_ptr<Signal> signal : signals_)
		signal->set_segment_display_mode(mode);

	uint32_t last_segment = session_.get_segment_count() - 1;

	switch (mode) {
	case Trace::ShowLastSegmentOnly:
		if (current_segment_ != last_segment)
			set_current_segment(last_segment);
		break;

	case Trace::ShowLastCompleteSegmentOnly:
		// Do nothing if we only have one segment so far
		if (last_segment > 0) {
			// If the last segment isn't complete, the previous one must be
			uint32_t segment_id =
				(session_.all_segments_complete(last_segment)) ?
				last_segment : last_segment - 1;

			if (current_segment_ != segment_id)
				set_current_segment(segment_id);
		}
		break;

	case Trace::ShowSingleSegmentOnly:
	case Trace::ShowAllSegments:
	case Trace::ShowAccumulatedIntensity:
	default:
		// Current segment remains as-is
		break;
	}

	segment_selectable_ = true;

	if ((mode == Trace::ShowAllSegments) || (mode == Trace::ShowAccumulatedIntensity))
		segment_selectable_ = false;

	viewport_->update();

	segment_display_mode_changed((int)mode, segment_selectable_);
}

void View::zoom(double steps)
{
	zoom(steps, viewport_->width() / 2);
}

void View::zoom(double steps, int offset)
{
	set_zoom(scale_ * pow(3.0 / 2.0, -steps), offset);
}

void View::zoom_fit(bool gui_state)
{
	// Act as one-shot when stopped, toggle along with the GUI otherwise
	if (session_.get_capture_state() == Session::Stopped) {
		always_zoom_to_fit_ = false;
		always_zoom_to_fit_changed(false);
	} else {
		always_zoom_to_fit_ = gui_state;
		always_zoom_to_fit_changed(gui_state);
	}

	const pair<Timestamp, Timestamp> extents = get_time_extents();
	const Timestamp delta = extents.second - extents.first;
	if (delta < Timestamp("1e-12"))
		return;

	assert(viewport_);
	const int w = viewport_->width();
	if (w <= 0)
		return;

	const Timestamp scale = max(min(delta / w, MaxScale), MinScale);
	set_scale_offset(scale.convert_to<double>(), extents.first);
}

void View::set_scale_offset(double scale, const Timestamp& offset)
{
	// Disable sticky scrolling / always zoom to fit when acquisition runs
	// and user drags the viewport
	if ((scale_ == scale) && (offset_ != offset) &&
			(session_.get_capture_state() == Session::Running)) {

		if (sticky_scrolling_) {
			sticky_scrolling_ = false;
			sticky_scrolling_changed(false);
		}

		if (always_zoom_to_fit_) {
			always_zoom_to_fit_ = false;
			always_zoom_to_fit_changed(false);
		}
	}

	set_scale(scale);
	set_offset(offset);

	calculate_tick_spacing();

	update_scroll();
	ruler_->update();
	viewport_->update();
}

set< shared_ptr<SignalData> > View::get_visible_data() const
{
	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data;
	for (const shared_ptr<Signal> sig : signals_)
		if (sig->enabled())
			visible_data.insert(sig->data());

	return visible_data;
}

pair<Timestamp, Timestamp> View::get_time_extents() const
{
	boost::optional<Timestamp> left_time, right_time;
	const set< shared_ptr<SignalData> > visible_data = get_visible_data();
	for (const shared_ptr<SignalData> d : visible_data) {
		const vector< shared_ptr<Segment> > segments = d->segments();
		for (const shared_ptr<Segment> &s : segments) {
			double samplerate = s->samplerate();
			samplerate = (samplerate <= 0.0) ? 1.0 : samplerate;

			const Timestamp start_time = s->start_time();
			left_time = left_time ?
				min(*left_time, start_time) :
				                start_time;
			right_time = right_time ?
				max(*right_time, start_time + d->max_sample_count() / samplerate) :
				                 start_time + d->max_sample_count() / samplerate;
		}
	}

	if (!left_time || !right_time)
		return make_pair(0, 0);

	assert(*left_time < *right_time);
	return make_pair(*left_time, *right_time);
}

void View::enable_show_sampling_points(bool state)
{
	(void)state;

	viewport_->update();
}

void View::enable_show_analog_minor_grid(bool state)
{
	(void)state;

	viewport_->update();
}

void View::enable_colored_bg(bool state)
{
	colored_bg_ = state;
	viewport_->update();
}

bool View::colored_bg() const
{
	return colored_bg_;
}

bool View::cursors_shown() const
{
	return show_cursors_;
}

void View::show_cursors(bool show)
{
	show_cursors_ = show;
	cursor_state_changed(show);
	ruler_->update();
	viewport_->update();
}

void View::centre_cursors()
{
	if (cursors_) {
		const double time_width = scale_ * viewport_->width();
		cursors_->first()->set_time(offset_ + time_width * 0.4);
		cursors_->second()->set_time(offset_ + time_width * 0.6);

		ruler_->update();
		viewport_->update();
	}
}

shared_ptr<CursorPair> View::cursors() const
{
	return cursors_;
}

void View::add_flag(const Timestamp& time)
{
	flags_.push_back(make_shared<Flag>(*this, time,
		QString("%1").arg(next_flag_text_)));

	next_flag_text_ = (next_flag_text_ >= 'Z') ? 'A' :
		(next_flag_text_ + 1);

	time_item_appearance_changed(true, true);
}

void View::remove_flag(shared_ptr<Flag> flag)
{
	flags_.remove(flag);
	time_item_appearance_changed(true, true);
}

vector< shared_ptr<Flag> > View::flags() const
{
	vector< shared_ptr<Flag> > flags(flags_.begin(), flags_.end());
	stable_sort(flags.begin(), flags.end(),
		[](const shared_ptr<Flag> &a, const shared_ptr<Flag> &b) {
			return a->time() < b->time();
		});

	return flags;
}

const QPoint& View::hover_point() const
{
	return hover_point_;
}

int64_t View::get_nearest_level_change(const QPoint &p) const
{
	if (snap_distance_ == 0)
		return -1;

	shared_ptr<Signal> signal = signal_under_mouse_cursor_;

	if (!signal)
		return -1;

	// Calculate sample number from cursor position
	const double samples_per_pixel = signal->base()->get_samplerate() * scale();
	const int64_t x_offset = offset().convert_to<double>() / scale();
	const int64_t sample_num = max(((x_offset + p.x()) * samples_per_pixel), 0.0);

	// Query for nearest level changes
	vector<data::LogicSegment::EdgePair> edges =
		signal->get_nearest_level_changes(sample_num);

	if (edges.size() != 2)
		return -1;

	// We received absolute sample numbers, make them relative
	const int64_t left_sample_delta = sample_num - edges.front().first;
	const int64_t right_sample_delta = edges.back().first - sample_num - 1;

	const int64_t left_delta = left_sample_delta / samples_per_pixel;
	const int64_t right_delta = right_sample_delta / samples_per_pixel;

	int64_t nearest = -1;

	// Only use closest left or right edge if they're close to the cursor
	if ((left_delta < right_delta) && (left_delta < snap_distance_))
		nearest = edges.front().first;
	if ((left_delta >= right_delta) && (right_delta < snap_distance_))
		nearest = edges.back().first;

	return nearest;
}

void View::restack_all_trace_tree_items()
{
	// Make a list of owners that is sorted from deepest first
	const vector<shared_ptr<TraceTreeItem>> items(
		list_by_type<TraceTreeItem>());
	set< TraceTreeItemOwner* > owners;
	for (const auto &r : items)
		owners.insert(r->owner());
	vector< TraceTreeItemOwner* > sorted_owners(owners.begin(), owners.end());
	sort(sorted_owners.begin(), sorted_owners.end(),
		[](const TraceTreeItemOwner* a, const TraceTreeItemOwner *b) {
			return a->depth() > b->depth(); });

	// Restack the items recursively
	for (auto &o : sorted_owners)
		o->restack_items();

	// Animate the items to their destination
	for (const auto &i : items)
		i->animate_to_layout_v_offset();
}

int View::header_width() const
{
	 return header_->extended_size_hint().width();
}

void View::on_setting_changed(const QString &key, const QVariant &value)
{
	if (key == GlobalSettings::Key_View_TriggerIsZeroTime)
		on_settingViewTriggerIsZeroTime_changed(value);

	if (key == GlobalSettings::Key_View_SnapDistance) {
		GlobalSettings settings;
		snap_distance_ = settings.value(GlobalSettings::Key_View_SnapDistance).toInt();
	}
}

void View::trigger_event(int segment_id, util::Timestamp location)
{
	// TODO This doesn't work if we're showing multiple segments at once
	if ((uint32_t)segment_id != current_segment_)
		return;

	// Set zero location if the Key_View_TriggerIsZeroTime setting is set and
	// if this is the first trigger for this segment.
	GlobalSettings settings;
	bool trigger_is_zero_time = settings.value(GlobalSettings::Key_View_TriggerIsZeroTime).toBool();

	size_t trigger_count = session_.get_triggers(current_segment_).size();

	if (trigger_is_zero_time && trigger_count == 1)
		set_zero_position(location);

	trigger_markers_.push_back(make_shared<TriggerMarker>(*this, location));
}

void View::get_scroll_layout(double &length, Timestamp &offset) const
{
	const pair<Timestamp, Timestamp> extents = get_time_extents();
	length = ((extents.second - extents.first) / scale_).convert_to<double>();
	offset = offset_ / scale_;
}

void View::set_zoom(double scale, int offset)
{
	// Reset the "always zoom to fit" feature as the user changed the zoom
	always_zoom_to_fit_ = false;
	always_zoom_to_fit_changed(false);

	const Timestamp cursor_offset = offset_ + scale_ * offset;
	const Timestamp new_scale = max(min(Timestamp(scale), MaxScale), MinScale);
	const Timestamp new_offset = cursor_offset - new_scale * offset;
	set_scale_offset(new_scale.convert_to<double>(), new_offset);
}

void View::calculate_tick_spacing()
{
	const double SpacingIncrement = 10.0f;
	const double MinValueSpacing = 40.0f;

	// Figure out the highest numeric value visible on a label
	const QSize areaSize = viewport_->size();
	const Timestamp max_time = max(fabs(offset_),
		fabs(offset_ + scale_ * areaSize.width()));

	double min_width = SpacingIncrement;
	double label_width, tick_period_width;

	QFontMetrics m(QApplication::font());

	// Copies of the member variables with the same name, used in the calculation
	// and written back afterwards, so that we don't emit signals all the time
	// during the calculation.
	pv::util::Timestamp tick_period = tick_period_;
	pv::util::SIPrefix tick_prefix = tick_prefix_;
	unsigned tick_precision = tick_precision_;

	do {
		const double min_period = scale_ * min_width;

		const int order = (int)floorf(log10f(min_period));
		const pv::util::Timestamp order_decimal =
			pow(pv::util::Timestamp(10), order);

		// Allow for a margin of error so that a scale unit of 1 can be used.
		// Otherwise, for a SU of 1 the tick period will almost always be below
		// the min_period by a small amount - and thus skipped in favor of 2.
		// Note: margin assumes that SU[0] and SU[1] contain the smallest values
		double tp_margin = (ScaleUnits[0] + ScaleUnits[1]) / 2.0;
		double tp_with_margin;
		unsigned int unit = 0;

		do {
			tp_with_margin = order_decimal.convert_to<double>() *
				(ScaleUnits[unit++] + tp_margin);
		} while (tp_with_margin < min_period && unit < countof(ScaleUnits));

		minor_tick_count_ = (unit == 2) ? 4 : 5;
		tick_period = order_decimal * ScaleUnits[unit - 1];
		tick_prefix = static_cast<pv::util::SIPrefix>(
			(order - pv::util::exponent(pv::util::SIPrefix::yocto)) / 3);

		// Precision is the number of fractional digits required, not
		// taking the prefix into account (and it must never be negative)
		tick_precision = max(ceil(log10(1 / tick_period)).convert_to<int>(), 0);

		tick_period_width = (tick_period / scale_).convert_to<double>();

		const QString label_text = Ruler::format_time_with_distance(
			tick_period, max_time, tick_prefix, time_unit_, tick_precision);

		label_width = m.boundingRect(0, 0, INT_MAX, INT_MAX,
			Qt::AlignLeft | Qt::AlignTop, label_text).width() +
				MinValueSpacing;

		min_width += SpacingIncrement;
	} while (tick_period_width < label_width);

	set_tick_period(tick_period);
	set_tick_prefix(tick_prefix);
	set_tick_precision(tick_precision);
}

void View::adjust_top_margin()
{
	assert(viewport_);

	const QSize areaSize = viewport_->size();

	const pair<int, int> extents = v_extents();
	const int top_margin = owner_visual_v_offset() + extents.first;
	const int trace_bottom = owner_visual_v_offset() + extents.first + extents.second;

	// Do we have empty space at the top while the last trace goes out of screen?
	if ((top_margin > 0) && (trace_bottom > areaSize.height())) {
		const int trace_height = extents.second - extents.first;

		// Center everything vertically if there is enough space
		if (areaSize.height() >= trace_height)
			set_v_offset(extents.first -
				((areaSize.height() - trace_height) / 2));
		else
			// Remove the top margin to make as many traces fit on screen as possible
			set_v_offset(extents.first);
	}
}

void View::update_scroll()
{
	assert(viewport_);
	QScrollBar *hscrollbar = scrollarea_->horizontalScrollBar();
	QScrollBar *vscrollbar = scrollarea_->verticalScrollBar();

	const QSize areaSize = viewport_->size();

	// Set the horizontal scroll bar
	double length = 0;
	Timestamp offset;
	get_scroll_layout(length, offset);
	length = max(length - areaSize.width(), 0.0);

	int major_tick_distance = (tick_period_ / scale_).convert_to<int>();

	hscrollbar->setPageStep(areaSize.width() / 2);
	hscrollbar->setSingleStep(major_tick_distance);

	updating_scroll_ = true;

	if (length < MaxScrollValue) {
		hscrollbar->setRange(0, length);
		hscrollbar->setSliderPosition(offset.convert_to<double>());
	} else {
		hscrollbar->setRange(0, MaxScrollValue);
		hscrollbar->setSliderPosition(
			(offset_ * MaxScrollValue / (scale_ * length)).convert_to<double>());
	}

	updating_scroll_ = false;

	// Set the vertical scrollbar
	vscrollbar->setPageStep(areaSize.height());
	vscrollbar->setSingleStep(areaSize.height() / 8);

	const pair<int, int> extents = v_extents();

	// Don't change the scrollbar range if there are no traces
	if (extents.first != extents.second)
		vscrollbar->setRange(extents.first - areaSize.height(),
			extents.second);

	if (scroll_needs_defaults_) {
		set_scroll_default();
		scroll_needs_defaults_ = false;
	}
}

void View::reset_scroll()
{
	scrollarea_->verticalScrollBar()->setRange(0, 0);
}

void View::set_scroll_default()
{
	assert(viewport_);

	const QSize areaSize = viewport_->size();

	const pair<int, int> extents = v_extents();
	const int trace_height = extents.second - extents.first;

	// Do all traces fit in the view?
	if (areaSize.height() >= trace_height)
		// Center all traces vertically
		set_v_offset(extents.first -
			((areaSize.height() - trace_height) / 2));
	else
		// Put the first trace at the top, letting the bottom ones overflow
		set_v_offset(extents.first);
}

void View::determine_if_header_was_shrunk()
{
	const int header_pane_width = splitter_->sizes().front();

	// Allow for a slight margin of error so that we also accept
	// slight differences when e.g. a label name change increased
	// the overall width
	header_was_shrunk_ = (header_pane_width < (header_width() - 10));
}

void View::resize_header_to_fit()
{
	// Setting the maximum width of the header widget doesn't work as
	// expected because the splitter would allow the user to make the
	// pane wider than that, creating empty space as a result.
	// To make this work, we stricly enforce the maximum width by
	// expanding the header unless the user shrunk it on purpose.
	// As we're then setting the width of the header pane, we set the
	// splitter to the maximum allowed position.

	int splitter_area_width = 0;
	for (int w : splitter_->sizes())
		splitter_area_width += w;

	// Make sure the header has enough horizontal space to show all labels fully
	QList<int> pane_sizes;
	pane_sizes.push_back(header_->extended_size_hint().width());
	pane_sizes.push_back(splitter_area_width - header_->extended_size_hint().width());
	splitter_->setSizes(pane_sizes);
}

void View::update_layout()
{
	update_scroll();
}

TraceTreeItemOwner* View::find_prevalent_trace_group(
	const shared_ptr<sigrok::ChannelGroup> &group,
	const unordered_map<shared_ptr<data::SignalBase>, shared_ptr<Signal> >
		&signal_map)
{
	assert(group);

	unordered_set<TraceTreeItemOwner*> owners;
	vector<TraceTreeItemOwner*> owner_list;

	// Make a set and a list of all the owners
	for (const auto &channel : group->channels()) {
		for (auto entry : signal_map) {
			if (entry.first->channel() == channel) {
				TraceTreeItemOwner *const o = (entry.second)->owner();
				owner_list.push_back(o);
				owners.insert(o);
			}
		}
	}

	// Iterate through the list of owners, and find the most prevalent
	size_t max_prevalence = 0;
	TraceTreeItemOwner *prevalent_owner = nullptr;
	for (TraceTreeItemOwner *owner : owners) {
		const size_t prevalence = count_if(
			owner_list.begin(), owner_list.end(),
			[&](TraceTreeItemOwner *o) { return o == owner; });
		if (prevalence > max_prevalence) {
			max_prevalence = prevalence;
			prevalent_owner = owner;
		}
	}

	return prevalent_owner;
}

vector< shared_ptr<Trace> > View::extract_new_traces_for_channels(
	const vector< shared_ptr<sigrok::Channel> > &channels,
	const unordered_map<shared_ptr<data::SignalBase>, shared_ptr<Signal> >
		&signal_map,
	set< shared_ptr<Trace> > &add_list)
{
	vector< shared_ptr<Trace> > filtered_traces;

	for (const auto &channel : channels) {
		for (auto entry : signal_map) {
			if (entry.first->channel() == channel) {
				shared_ptr<Trace> trace = entry.second;
				const auto list_iter = add_list.find(trace);
				if (list_iter == add_list.end())
					continue;

				filtered_traces.push_back(trace);
				add_list.erase(list_iter);
			}
		}
	}

	return filtered_traces;
}

void View::determine_time_unit()
{
	// Check whether we know the sample rate and hence can use time as the unit
	if (time_unit_ == util::TimeUnit::Samples) {
		// Check all signals but...
		for (const shared_ptr<Signal> signal : signals_) {
			const shared_ptr<SignalData> data = signal->data();

			// ...only check first segment of each
			const vector< shared_ptr<Segment> > segments = data->segments();
			if (!segments.empty())
				if (segments[0]->samplerate()) {
					set_time_unit(util::TimeUnit::Time);
					break;
				}
		}
	}
}

bool View::eventFilter(QObject *object, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type == QEvent::MouseMove) {

		const QMouseEvent *const mouse_event = (QMouseEvent*)event;
		if (object == viewport_)
			hover_point_ = mouse_event->pos();
		else if (object == ruler_)
			// Adjust the hover point's y coordinate so that it's relative to
			// the top of the viewport. The result may be negative.
			hover_point_ = QPoint(mouse_event->pos().x(),
				mouse_event->pos().y() - ruler_->sizeHint().height());
		else if (object == header_)
			hover_point_ = QPoint(0, mouse_event->y());
		else
			hover_point_ = QPoint(-1, -1);

		update_hover_point();

	} else if (type == QEvent::Leave) {
		hover_point_ = QPoint(-1, -1);
		update_hover_point();
	} else if (type == QEvent::Show) {

		// This is somewhat of a hack, unfortunately. We cannot use
		// set_v_offset() from within restore_settings() as the view
		// at that point is neither visible nor properly sized.
		// This is the least intrusive workaround I could come up
		// with: set the vertical offset (or scroll defaults) when
		// the view is shown, which happens after all widgets were
		// resized to their final sizes.
		update_layout();

		if (settings_restored_)
			determine_if_header_was_shrunk();
		else
			resize_header_to_fit();

		if (scroll_needs_defaults_) {
			set_scroll_default();
			scroll_needs_defaults_ = false;
		}

		if (saved_v_offset_) {
			set_v_offset(saved_v_offset_);
			saved_v_offset_ = 0;
		}
	}

	return QObject::eventFilter(object, event);
}

void View::contextMenuEvent(QContextMenuEvent *event)
{
	QPoint pos = event->pos() - QPoint(0, ruler_->sizeHint().height());

	const shared_ptr<ViewItem> r = viewport_->get_mouse_over_item(pos);
	if (!r)
		return;

	QMenu *menu = r->create_view_context_menu(this, pos);
	if (menu)
		menu->exec(event->globalPos());
}

void View::resizeEvent(QResizeEvent* event)
{
	// Only adjust the top margin if we shrunk vertically
	if (event->size().height() < event->oldSize().height())
		adjust_top_margin();

	update_layout();
}

void View::update_hover_point()
{
	// Determine signal that the mouse cursor is hovering over
	signal_under_mouse_cursor_.reset();
	for (shared_ptr<Signal> s : signals_) {
		const pair<int, int> extents = s->v_extents();
		const int top = s->get_visual_y() + extents.first;
		const int btm = s->get_visual_y() + extents.second;
		if ((hover_point_.y() >= top) && (hover_point_.y() <= btm)
			&& s->base()->enabled())
			signal_under_mouse_cursor_ = s;
	}

	// Update all trace tree items
	const vector<shared_ptr<TraceTreeItem>> trace_tree_items(
		list_by_type<TraceTreeItem>());
	for (shared_ptr<TraceTreeItem> r : trace_tree_items)
		r->hover_point_changed(hover_point_);

	// Notify any other listeners
	hover_point_changed(hover_point_);
}

void View::row_item_appearance_changed(bool label, bool content)
{
	if (label)
		header_->update();
	if (content)
		viewport_->update();
}

void View::time_item_appearance_changed(bool label, bool content)
{
	if (label) {
		ruler_->update();

		// Make sure the header pane width is updated, too
		update_layout();
	}

	if (content)
		viewport_->update();
}

void View::extents_changed(bool horz, bool vert)
{
	sticky_events_ |=
		(horz ? TraceTreeItemHExtentsChanged : 0) |
		(vert ? TraceTreeItemVExtentsChanged : 0);

	lazy_event_handler_.stop();
	lazy_event_handler_.start();
}

void View::on_signal_name_changed()
{
	if (!header_was_shrunk_)
		resize_header_to_fit();
}

void View::on_splitter_moved()
{
	// The header can only shrink when the splitter is moved manually
	determine_if_header_was_shrunk();

	if (!header_was_shrunk_)
		resize_header_to_fit();
}

void View::h_scroll_value_changed(int value)
{
	if (updating_scroll_)
		return;

	// Disable sticky scrolling when user moves the horizontal scroll bar
	// during a running acquisition
	if (sticky_scrolling_ && (session_.get_capture_state() == Session::Running)) {
		sticky_scrolling_ = false;
		sticky_scrolling_changed(false);
	}

	const int range = scrollarea_->horizontalScrollBar()->maximum();
	if (range < MaxScrollValue)
		set_offset(scale_ * value);
	else {
		double length = 0;
		Timestamp offset;
		get_scroll_layout(length, offset);
		set_offset(scale_ * length * value / MaxScrollValue);
	}

	ruler_->update();
	viewport_->update();
}

void View::v_scroll_value_changed()
{
	header_->update();
	viewport_->update();
}

void View::signals_changed()
{
	using sigrok::Channel;

	vector< shared_ptr<Channel> > channels;
	shared_ptr<sigrok::Device> sr_dev;
	bool signals_added_or_removed = false;

	// Do we need to set the vertical scrollbar to its default position later?
	// We do if there are no traces, i.e. the scroll bar has no range set
	bool reset_scrollbar =
		(scrollarea_->verticalScrollBar()->minimum() ==
			scrollarea_->verticalScrollBar()->maximum());

	if (!session_.device()) {
		reset_scroll();
		signals_.clear();
	} else {
		sr_dev = session_.device()->device();
		assert(sr_dev);
		channels = sr_dev->channels();
	}

	vector< shared_ptr<TraceTreeItem> > new_top_level_items;

	// Make a list of traces that are being added, and a list of traces
	// that are being removed
	const vector<shared_ptr<Trace>> prev_trace_list = list_by_type<Trace>();
	const set<shared_ptr<Trace>> prev_traces(
		prev_trace_list.begin(), prev_trace_list.end());

	set< shared_ptr<Trace> > traces(signals_.begin(), signals_.end());

#ifdef ENABLE_DECODE
	traces.insert(decode_traces_.begin(), decode_traces_.end());
#endif

	set< shared_ptr<Trace> > add_traces;
	set_difference(traces.begin(), traces.end(),
		prev_traces.begin(), prev_traces.end(),
		inserter(add_traces, add_traces.begin()));

	set< shared_ptr<Trace> > remove_traces;
	set_difference(prev_traces.begin(), prev_traces.end(),
		traces.begin(), traces.end(),
		inserter(remove_traces, remove_traces.begin()));

	// Make a look-up table of sigrok Channels to pulseview Signals
	unordered_map<shared_ptr<data::SignalBase>, shared_ptr<Signal> >
		signal_map;
	for (const shared_ptr<Signal> &sig : signals_)
		signal_map[sig->base()] = sig;

	// Populate channel groups
	if (sr_dev)
		for (auto entry : sr_dev->channel_groups()) {
			const shared_ptr<sigrok::ChannelGroup> &group = entry.second;

			if (group->channels().size() <= 1)
				continue;

			// Find best trace group to add to
			TraceTreeItemOwner *owner = find_prevalent_trace_group(
				group, signal_map);

			// If there is no trace group, create one
			shared_ptr<TraceGroup> new_trace_group;
			if (!owner) {
				new_trace_group.reset(new TraceGroup());
				owner = new_trace_group.get();
			}

			// Extract traces for the trace group, removing them from
			// the add list
			const vector< shared_ptr<Trace> > new_traces_in_group =
				extract_new_traces_for_channels(group->channels(),
					signal_map, add_traces);

			// Add the traces to the group
			const pair<int, int> prev_v_extents = owner->v_extents();
			int offset = prev_v_extents.second - prev_v_extents.first;
			for (shared_ptr<Trace> trace : new_traces_in_group) {
				assert(trace);
				owner->add_child_item(trace);

				const pair<int, int> extents = trace->v_extents();
				if (trace->enabled())
					offset += -extents.first;
				trace->force_to_v_offset(offset);
				if (trace->enabled())
					offset += extents.second;
			}

			if (new_trace_group) {
				// Assign proper vertical offsets to each channel in the group
				new_trace_group->restack_items();

				// If this is a new group, enqueue it in the new top level
				// items list
				if (!new_traces_in_group.empty())
					new_top_level_items.push_back(new_trace_group);
			}
		}

	// Enqueue the remaining logic channels in a group
	vector< shared_ptr<Channel> > logic_channels;
	copy_if(channels.begin(), channels.end(), back_inserter(logic_channels),
		[](const shared_ptr<Channel>& c) {
			return c->type() == sigrok::ChannelType::LOGIC; });

	const vector< shared_ptr<Trace> > non_grouped_logic_signals =
		extract_new_traces_for_channels(logic_channels,	signal_map, add_traces);

	if (non_grouped_logic_signals.size() > 0) {
		const shared_ptr<TraceGroup> non_grouped_trace_group(
			make_shared<TraceGroup>());
		for (shared_ptr<Trace> trace : non_grouped_logic_signals)
			non_grouped_trace_group->add_child_item(trace);

		non_grouped_trace_group->restack_items();
		new_top_level_items.push_back(non_grouped_trace_group);
	}

	// Enqueue the remaining channels as free ungrouped traces
	const vector< shared_ptr<Trace> > new_top_level_signals =
		extract_new_traces_for_channels(channels, signal_map, add_traces);
	new_top_level_items.insert(new_top_level_items.end(),
		new_top_level_signals.begin(), new_top_level_signals.end());

	// Enqueue any remaining traces i.e. decode traces
	new_top_level_items.insert(new_top_level_items.end(),
		add_traces.begin(), add_traces.end());

	// Remove any removed traces
	for (shared_ptr<Trace> trace : remove_traces) {
		TraceTreeItemOwner *const owner = trace->owner();
		assert(owner);
		owner->remove_child_item(trace);
		signals_added_or_removed = true;
	}

	// Remove any empty trace groups
	for (shared_ptr<TraceGroup> group : list_by_type<TraceGroup>())
		if (group->child_items().size() == 0) {
			remove_child_item(group);
			group.reset();
		}

	// Add and position the pending top levels items
	int offset = v_extents().second;
	for (auto item : new_top_level_items) {
		add_child_item(item);

		// Position the item after the last item or at the top if there is none
		const pair<int, int> extents = item->v_extents();

		if (item->enabled())
			offset += -extents.first;

		item->force_to_v_offset(offset);

		if (item->enabled())
			offset += extents.second;
		signals_added_or_removed = true;
	}


	if (signals_added_or_removed && !header_was_shrunk_)
		resize_header_to_fit();

	update_layout();

	header_->update();
	viewport_->update();

	if (reset_scrollbar)
		set_scroll_default();
}

void View::capture_state_updated(int state)
{
	GlobalSettings settings;

	if (state == Session::Running) {
		set_time_unit(util::TimeUnit::Samples);

		trigger_markers_.clear();

		scale_at_acq_start_ = scale_;
		offset_at_acq_start_ = offset_;

		// Activate "always zoom to fit" if the setting is enabled and we're
		// the main view of this session (other trace views may be used for
		// zooming and we don't want to mess them up)
		bool state = settings.value(GlobalSettings::Key_View_ZoomToFitDuringAcq).toBool();
		if (is_main_view_ && state) {
			always_zoom_to_fit_ = true;
			always_zoom_to_fit_changed(always_zoom_to_fit_);
		}

		// Enable sticky scrolling if the setting is enabled
		sticky_scrolling_ = settings.value(GlobalSettings::Key_View_StickyScrolling).toBool();

		// Reset all traces to segment 0
		current_segment_ = 0;
		set_current_segment(current_segment_);
	}

	if (state == Session::Stopped) {
		// After acquisition has stopped we need to re-calculate the ticks once
		// as it's otherwise done when the user pans or zooms, which is too late
		calculate_tick_spacing();

		// Reset "always zoom to fit", the acquisition has stopped
		if (always_zoom_to_fit_) {
			// Perform a final zoom-to-fit before disabling
			zoom_fit(always_zoom_to_fit_);
			always_zoom_to_fit_ = false;
			always_zoom_to_fit_changed(always_zoom_to_fit_);
		}

		bool zoom_to_fit_after_acq =
			settings.value(GlobalSettings::Key_View_ZoomToFitAfterAcq).toBool();

		// Only perform zoom-to-fit if the user hasn't altered the viewport and
		// we didn't restore settings in the meanwhile
		if (zoom_to_fit_after_acq &&
			!suppress_zoom_to_fit_after_acq_ &&
			(scale_ == scale_at_acq_start_) &&
			(offset_ == offset_at_acq_start_))
			zoom_fit(false);  // We're stopped, so the GUI state doesn't matter

		suppress_zoom_to_fit_after_acq_ = false;
	}
}

void View::on_new_segment(int new_segment_id)
{
	on_segment_changed(new_segment_id);
}

void View::on_segment_completed(int segment_id)
{
	on_segment_changed(segment_id);
}

void View::on_segment_changed(int segment)
{
	switch (segment_display_mode_) {
	case Trace::ShowLastSegmentOnly:
	case Trace::ShowSingleSegmentOnly:
		set_current_segment(segment);
		break;

	case Trace::ShowLastCompleteSegmentOnly:
		// Only update if all segments are complete
		if (session_.all_segments_complete(segment))
			set_current_segment(segment);
		break;

	case Trace::ShowAllSegments:
	case Trace::ShowAccumulatedIntensity:
	default:
		break;
	}
}

void View::on_settingViewTriggerIsZeroTime_changed(const QVariant new_value)
{
	if (new_value.toBool()) {
		// The first trigger for this segment is used as the zero position
		vector<util::Timestamp> triggers = session_.get_triggers(current_segment_);
		if (triggers.size() > 0)
			set_zero_position(triggers.front());
	} else
		reset_zero_position();
}

void View::perform_delayed_view_update()
{
	if (always_zoom_to_fit_) {
		zoom_fit(true);
	} else if (sticky_scrolling_) {
		// Make right side of the view sticky
		double length = 0;
		Timestamp offset;
		get_scroll_layout(length, offset);

		const QSize areaSize = viewport_->size();
		length = max(length - areaSize.width(), 0.0);

		set_offset(scale_ * length);
	}

	determine_time_unit();
	update_scroll();
	ruler_->update();
	viewport_->update();
}

void View::process_sticky_events()
{
	if (sticky_events_ & TraceTreeItemHExtentsChanged)
		update_layout();
	if (sticky_events_ & TraceTreeItemVExtentsChanged) {
		restack_all_trace_tree_items();
		update_scroll();
	}

	// Clear the sticky events
	sticky_events_ = 0;
}

} // namespace trace
} // namespace views
} // namespace pv
