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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <extdef.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <iterator>
#include <mutex>
#include <unordered_set>

#include <boost/thread/locks.hpp>

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QScrollBar>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "analogsignal.hpp"
#include "decodetrace.hpp"
#include "header.hpp"
#include "logicsignal.hpp"
#include "ruler.hpp"
#include "signal.hpp"
#include "tracegroup.hpp"
#include "triggermarker.hpp"
#include "view.hpp"
#include "viewport.hpp"

#include "pv/session.hpp"
#include "pv/devices/device.hpp"
#include "pv/data/logic.hpp"
#include "pv/data/logicsegment.hpp"
#include "pv/util.hpp"

using boost::shared_lock;
using boost::shared_mutex;

using pv::data::SignalData;
using pv::data::Segment;
using pv::util::TimeUnit;
using pv::util::Timestamp;

using std::back_inserter;
using std::copy_if;
using std::deque;
using std::dynamic_pointer_cast;
using std::inserter;
using std::list;
using std::lock_guard;
using std::max;
using std::make_pair;
using std::make_shared;
using std::min;
using std::pair;
using std::set;
using std::set_difference;
using std::shared_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::weak_ptr;

namespace pv {
namespace view {

const Timestamp View::MaxScale("1e9");
const Timestamp View::MinScale("1e-12");

const int View::MaxScrollValue = INT_MAX / 2;
const int View::MaxViewAutoUpdateRate = 25; // No more than 25 Hz with sticky scrolling

const int View::ScaleUnits[3] = {1, 2, 5};

View::View(Session &session, QWidget *parent) :
	QAbstractScrollArea(parent),
	session_(session),
	viewport_(new Viewport(*this)),
	ruler_(new Ruler(*this)),
	header_(new Header(*this)),
	scale_(1e-3),
	offset_(0),
	updating_scroll_(false),
	sticky_scrolling_(false), // Default setting is set in MainWindow::setup_ui()
	always_zoom_to_fit_(false),
	tick_period_(0),
	tick_prefix_(pv::util::SIPrefix::yocto),
	tick_precision_(0),
	time_unit_(util::TimeUnit::Time),
	show_cursors_(false),
	cursors_(new CursorPair(*this)),
	next_flag_text_('A'),
	trigger_markers_(),
	hover_point_(-1, -1)
{
	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed()));

	connect(&session_, SIGNAL(signals_changed()),
		this, SLOT(signals_changed()));
	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(capture_state_updated(int)));
	connect(&session_, SIGNAL(data_received()),
		this, SLOT(data_updated()));
	connect(&session_, SIGNAL(frame_ended()),
		this, SLOT(data_updated()));

	connect(header_, SIGNAL(selection_changed()),
		ruler_, SLOT(clear_selection()));
	connect(ruler_, SIGNAL(selection_changed()),
		header_, SLOT(clear_selection()));

	connect(header_, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));
	connect(ruler_, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));

	connect(this, SIGNAL(hover_point_changed()),
		this, SLOT(on_hover_point_changed()));

	connect(&lazy_event_handler_, SIGNAL(timeout()),
		this, SLOT(process_sticky_events()));
	lazy_event_handler_.setSingleShot(true);

	connect(&delayed_view_updater_, SIGNAL(timeout()),
		this, SLOT(perform_delayed_view_update()));
	delayed_view_updater_.setSingleShot(true);
	delayed_view_updater_.setInterval(1000 / MaxViewAutoUpdateRate);

	setViewport(viewport_);

	viewport_->installEventFilter(this);
	ruler_->installEventFilter(this);
	header_->installEventFilter(this);

	// Trigger the initial event manually. The default device has signals
	// which were created before this object came into being
	signals_changed();

	// make sure the transparent widgets are on the top
	ruler_->raise();
	header_->raise();

	// Update the zoom state
	calculate_tick_spacing();
}

Session& View::session()
{
	return session_;
}

const Session& View::session() const
{
	return session_;
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

vector< shared_ptr<TimeItem> > View::time_items() const
{
	const vector<shared_ptr<Flag>> f(flags());
	vector<shared_ptr<TimeItem>> items(f.begin(), f.end());
	items.push_back(cursors_);
	items.push_back(cursors_->first());
	items.push_back(cursors_->second());

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
		Q_EMIT scale_changed();
	}
}

const Timestamp& View::offset() const
{
	return offset_;
}

void View::set_offset(const pv::util::Timestamp& offset)
{
	if (offset_ != offset) {
		offset_ = offset;
		Q_EMIT offset_changed();
	}
}

int View::owner_visual_v_offset() const
{
	return -verticalScrollBar()->sliderPosition();
}

void View::set_v_offset(int offset)
{
	verticalScrollBar()->setSliderPosition(offset);
	header_->update();
	viewport_->update();
}

unsigned int View::depth() const
{
	return 0;
}

pv::util::SIPrefix View::tick_prefix() const
{
	return tick_prefix_;
}

void View::set_tick_prefix(pv::util::SIPrefix tick_prefix)
{
	if (tick_prefix_ != tick_prefix) {
		tick_prefix_ = tick_prefix;
		Q_EMIT tick_prefix_changed();
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
		Q_EMIT tick_precision_changed();
	}
}

const pv::util::Timestamp& View::tick_period() const
{
	return tick_period_;
}

void View::set_tick_period(const pv::util::Timestamp& tick_period)
{
	if (tick_period_ != tick_period) {
		tick_period_ = tick_period;
		Q_EMIT tick_period_changed();
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
		Q_EMIT time_unit_changed();
	}
}

void View::zoom(double steps)
{
	zoom(steps, viewport_->width() / 2);
}

void View::zoom(double steps, int offset)
{
	set_zoom(scale_ * pow(3.0/2.0, -steps), offset);
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

void View::zoom_one_to_one()
{
	using pv::data::SignalData;

	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data = get_visible_data();
	if (visible_data.empty())
		return;

	assert(viewport_);
	const int w = viewport_->width();
	if (w <= 0)
		return;

	set_zoom(1.0 / session_.get_samplerate(), w / 2);
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
	const unordered_set< shared_ptr<Signal> > sigs(session().signals());

	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data;
	for (const shared_ptr<Signal> sig : sigs)
		if (sig->enabled())
			visible_data.insert(sig->data());

	return visible_data;
}

pair<Timestamp, Timestamp> View::get_time_extents() const
{
	boost::optional<Timestamp> left_time, right_time;
	const set< shared_ptr<SignalData> > visible_data = get_visible_data();
	for (const shared_ptr<SignalData> d : visible_data) {
		const vector< shared_ptr<Segment> > segments =
			d->segments();
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

void View::enable_sticky_scrolling(bool state)
{
	sticky_scrolling_ = state;
}

void View::enable_coloured_bg(bool state)
{
	const vector<shared_ptr<TraceTreeItem>> items(
		list_by_type<TraceTreeItem>());

	for (shared_ptr<TraceTreeItem> i : items) {
		// Can't cast to Trace because it's abstract, so we need to
		// check for any derived classes individually

		shared_ptr<AnalogSignal> a = dynamic_pointer_cast<AnalogSignal>(i);
		if (a)
			a->set_coloured_bg(state);

		shared_ptr<LogicSignal> l = dynamic_pointer_cast<LogicSignal>(i);
		if (l)
			l->set_coloured_bg(state);

		shared_ptr<DecodeTrace> d = dynamic_pointer_cast<DecodeTrace>(i);
		if (d)
			d->set_coloured_bg(state);
	}

	viewport_->update();
}

bool View::cursors_shown() const
{
	return show_cursors_;
}

void View::show_cursors(bool show)
{
	show_cursors_ = show;
	ruler_->update();
	viewport_->update();
}

void View::centre_cursors()
{
	const double time_width = scale_ * viewport_->width();
	cursors_->first()->set_time(offset_ + time_width * 0.4);
	cursors_->second()->set_time(offset_ + time_width * 0.6);
	ruler_->update();
	viewport_->update();
}

std::shared_ptr<CursorPair> View::cursors() const
{
	return cursors_;
}

void View::add_flag(const Timestamp& time)
{
	flags_.push_back(shared_ptr<Flag>(new Flag(*this, time,
		QString("%1").arg(next_flag_text_))));

	next_flag_text_ = (next_flag_text_ >= 'Z') ? 'A' :
		(next_flag_text_ + 1);

	time_item_appearance_changed(true, true);
}

void View::remove_flag(std::shared_ptr<Flag> flag)
{
	flags_.remove(flag);
	time_item_appearance_changed(true, true);
}

vector< std::shared_ptr<Flag> > View::flags() const
{
	vector< std::shared_ptr<Flag> > flags(flags_.begin(), flags_.end());
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

void View::update_viewport()
{
	assert(viewport_);
	viewport_->update();
	header_->update();
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

void View::trigger_event(util::Timestamp location)
{
	trigger_markers_.push_back(shared_ptr<TriggerMarker>(
		new TriggerMarker(*this, location)));
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

		tick_period = order_decimal * ScaleUnits[unit - 1];
		tick_prefix = static_cast<pv::util::SIPrefix>(
			(order - pv::util::exponent(pv::util::SIPrefix::yocto)) / 3);

		// Precision is the number of fractional digits required, not
		// taking the prefix into account (and it must never be negative)
		tick_precision = std::max(ceil(log10(1 / tick_period)).convert_to<int>(), 0);

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

void View::update_scroll()
{
	assert(viewport_);

	const QSize areaSize = viewport_->size();

	// Set the horizontal scroll bar
	double length = 0;
	Timestamp offset;
	get_scroll_layout(length, offset);
	length = max(length - areaSize.width(), 0.0);

	int major_tick_distance = (tick_period_ / scale_).convert_to<int>();

	horizontalScrollBar()->setPageStep(areaSize.width() / 2);
	horizontalScrollBar()->setSingleStep(major_tick_distance);

	updating_scroll_ = true;

	if (length < MaxScrollValue) {
		horizontalScrollBar()->setRange(0, length);
		horizontalScrollBar()->setSliderPosition(offset.convert_to<double>());
	} else {
		horizontalScrollBar()->setRange(0, MaxScrollValue);
		horizontalScrollBar()->setSliderPosition(
			(offset_ * MaxScrollValue / (scale_ * length)).convert_to<double>());
	}

	updating_scroll_ = false;

	// Set the vertical scrollbar
	verticalScrollBar()->setPageStep(areaSize.height());
	verticalScrollBar()->setSingleStep(areaSize.height() / 8);

	const pair<int, int> extents = v_extents();
	verticalScrollBar()->setRange(extents.first - (areaSize.height() / 2),
		extents.second - (areaSize.height() / 2));
}

void View::update_layout()
{
	setViewportMargins(
		header_->sizeHint().width() - pv::view::Header::BaselineOffset,
		ruler_->sizeHint().height(), 0, 0);
	ruler_->setGeometry(viewport_->x(), 0,
		viewport_->width(), ruler_->extended_size_hint().height());
	header_->setGeometry(0, viewport_->y(),
		header_->extended_size_hint().width(), viewport_->height());
	update_scroll();
}

void View::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	(void)p;
	(void)rect;
	(void)hover;
}

QRectF View::label_rect(const QRectF &rect)
{
	(void)rect;
	return QRectF();
}

TraceTreeItemOwner* View::find_prevalent_trace_group(
	const shared_ptr<sigrok::ChannelGroup> &group,
	const unordered_map<shared_ptr<sigrok::Channel>, shared_ptr<Signal> >
		&signal_map)
{
	assert(group);

	unordered_set<TraceTreeItemOwner*> owners;
	vector<TraceTreeItemOwner*> owner_list;

	// Make a set and a list of all the owners
	for (const auto &channel : group->channels()) {
		const auto iter = signal_map.find(channel);
		if (iter == signal_map.end())
			continue;

		TraceTreeItemOwner *const o = (*iter).second->owner();
		owner_list.push_back(o);
		owners.insert(o);
	}

	// Iterate through the list of owners, and find the most prevalent
	size_t max_prevalence = 0;
	TraceTreeItemOwner *prevalent_owner = nullptr;
	for (TraceTreeItemOwner *owner : owners) {
		const size_t prevalence = std::count_if(
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
	const unordered_map<shared_ptr<sigrok::Channel>, shared_ptr<Signal> >
		&signal_map,
	set< shared_ptr<Trace> > &add_list)
{
	vector< shared_ptr<Trace> > filtered_traces;

	for (const auto &channel : channels) {
		const auto map_iter = signal_map.find(channel);
		if (map_iter == signal_map.end())
			continue;

		shared_ptr<Trace> trace = (*map_iter).second;
		const auto list_iter = add_list.find(trace);
		if (list_iter == add_list.end())
			continue;

		filtered_traces.push_back(trace);
		add_list.erase(list_iter);
	}

	return filtered_traces;
}

void View::determine_time_unit()
{
	// Check whether we know the sample rate and hence can use time as the unit
	if (time_unit_ == util::TimeUnit::Samples) {
		const unordered_set< shared_ptr<Signal> > sigs(session().signals());

		// Check all signals but...
		for (const shared_ptr<Signal> signal : sigs) {
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
			hover_point_ = QPoint(mouse_event->x(), 0);
		else if (object == header_)
			hover_point_ = QPoint(0, mouse_event->y());
		else
			hover_point_ = QPoint(-1, -1);

		hover_point_changed();

	} else if (type == QEvent::Leave) {
		hover_point_ = QPoint(-1, -1);
		hover_point_changed();
	}

	return QObject::eventFilter(object, event);
}

bool View::viewportEvent(QEvent *e)
{
	switch (e->type()) {
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
		return QAbstractScrollArea::viewportEvent(e);
	}
}

void View::resizeEvent(QResizeEvent*)
{
	update_layout();
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
	if (label)
		ruler_->update();
	if (content)
		viewport_->update();
}

void View::extents_changed(bool horz, bool vert)
{
	sticky_events_ |=
		(horz ? TraceTreeItemHExtentsChanged : 0) |
		(vert ? TraceTreeItemVExtentsChanged : 0);
	lazy_event_handler_.start();
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

	const int range = horizontalScrollBar()->maximum();
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

	vector< shared_ptr<TraceTreeItem> > new_top_level_items;

	const auto device = session_.device();
	if (!device)
		return;

	shared_ptr<sigrok::Device> sr_dev = device->device();
	assert(sr_dev);

	const vector< shared_ptr<Channel> > channels(
		sr_dev->channels());

	// Make a list of traces that are being added, and a list of traces
	// that are being removed
	const vector<shared_ptr<Trace>> prev_trace_list = list_by_type<Trace>();
	const set<shared_ptr<Trace>> prev_traces(
		prev_trace_list.begin(), prev_trace_list.end());

	const unordered_set< shared_ptr<Signal> > sigs(session_.signals());

	set< shared_ptr<Trace> > traces(sigs.begin(), sigs.end());

#ifdef ENABLE_DECODE
	const vector< shared_ptr<DecodeTrace> > decode_traces(
		session().get_decode_signals());
	traces.insert(decode_traces.begin(), decode_traces.end());
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
	unordered_map<shared_ptr<sigrok::Channel>, shared_ptr<Signal> >
		signal_map;
	for (const shared_ptr<Signal> &sig : sigs)
		signal_map[sig->channel()] = sig;

	// Populate channel groups
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

		// If this is a new group, enqueue it in the new top level
		// items list
		if (!new_traces_in_group.empty() && new_trace_group)
			new_top_level_items.push_back(new_trace_group);
	}

	// Enqueue the remaining logic channels in a group
	vector< shared_ptr<Channel> > logic_channels;
	copy_if(channels.begin(), channels.end(), back_inserter(logic_channels),
		[](const shared_ptr<Channel>& c) {
			return c->type() == sigrok::ChannelType::LOGIC; });
	const vector< shared_ptr<Trace> > non_grouped_logic_signals =
		extract_new_traces_for_channels(logic_channels,
			signal_map, add_traces);
	const shared_ptr<TraceGroup> non_grouped_trace_group(
		make_shared<TraceGroup>());
	for (shared_ptr<Trace> trace : non_grouped_logic_signals)
		non_grouped_trace_group->add_child_item(trace);
	new_top_level_items.push_back(non_grouped_trace_group);

	// Enqueue the remaining channels as free ungrouped traces
	const vector< shared_ptr<Trace> > new_top_level_signals =
		extract_new_traces_for_channels(channels,
			signal_map, add_traces);
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
	}

	// Add and position the pending top levels items
	for (auto item : new_top_level_items) {
		add_child_item(item);

		// Position the item after the last present item
		int offset = v_extents().second;
		const pair<int, int> extents = item->v_extents();
		if (item->enabled())
			offset += -extents.first;
		item->force_to_v_offset(offset);
		if (item->enabled())
			offset += extents.second;
	}

	update_layout();

	header_->update();
	viewport_->update();
}

void View::capture_state_updated(int state)
{
	if (state == Session::Running) {
		set_time_unit(util::TimeUnit::Samples);

		trigger_markers_.clear();
	}

	if (state == Session::Stopped) {
		// After acquisition has stopped we need to re-calculate the ticks once
		// as it's otherwise done when the user pans or zooms, which is too late
		calculate_tick_spacing();

		// Reset "always zoom to fit", the acquisition has stopped
		if (always_zoom_to_fit_) {
			always_zoom_to_fit_ = false;
			always_zoom_to_fit_changed(false);
		}
	}
}

void View::data_updated()
{
	if (always_zoom_to_fit_ || sticky_scrolling_) {
		if (!delayed_view_updater_.isActive())
			delayed_view_updater_.start();
	} else {
		determine_time_unit();
		update_scroll();
		ruler_->update();
		viewport_->update();
	}
}

void View::perform_delayed_view_update()
{
	if (always_zoom_to_fit_)
		zoom_fit(true);

	if (sticky_scrolling_) {
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

void View::on_hover_point_changed()
{
	const vector<shared_ptr<TraceTreeItem>> trace_tree_items(
		list_by_type<TraceTreeItem>());
	for (shared_ptr<TraceTreeItem> r : trace_tree_items)
		r->hover_point_changed();
}

} // namespace view
} // namespace pv
