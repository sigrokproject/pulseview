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

#include <cassert>
#include <climits>
#include <cmath>
#include <mutex>
#include <unordered_set>

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QScrollBar>

#include <libsigrok/libsigrok.hpp>

#include "cursorheader.hpp"
#include "decodetrace.hpp"
#include "header.hpp"
#include "logicsignal.hpp"
#include "ruler.hpp"
#include "signal.hpp"
#include "tracegroup.hpp"
#include "view.hpp"
#include "viewport.hpp"

#include "pv/session.hpp"
#include "pv/data/logic.hpp"
#include "pv/data/logicsnapshot.hpp"
#include "pv/util.hpp"

using boost::shared_lock;
using boost::shared_mutex;

using pv::data::SignalData;
using pv::data::Snapshot;
using pv::util::format_time;

using std::back_inserter;
using std::deque;
using std::dynamic_pointer_cast;
using std::list;
using std::lock_guard;
using std::max;
using std::make_pair;
using std::min;
using std::pair;
using std::set;
using std::shared_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::weak_ptr;

namespace pv {
namespace view {

const double View::MaxScale = 1e9;
const double View::MinScale = 1e-15;

const int View::MaxScrollValue = INT_MAX / 2;

const int View::ScaleUnits[3] = {1, 2, 5};

const QColor View::CursorAreaColour(220, 231, 243);

const QSizeF View::LabelPadding(4, 0);

View::View(Session &session, QWidget *parent) :
	QAbstractScrollArea(parent),
	session_(session),
	viewport_(new Viewport(*this)),
	ruler_(new Ruler(*this)),
	cursorheader_(new CursorHeader(*this)),
	header_(new Header(*this)),
	scale_(1e-6),
	offset_(0),
	v_offset_(0),
	updating_scroll_(false),
	tick_period_(0.0),
	tick_prefix_(0),
	show_cursors_(false),
	cursors_(*this),
	hover_point_(-1, -1)
{
	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed(int)));

	connect(&session_, SIGNAL(signals_changed()),
		this, SLOT(signals_changed()));
	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(data_updated()));
	connect(&session_, SIGNAL(data_received()),
		this, SLOT(data_updated()));
	connect(&session_, SIGNAL(frame_ended()),
		this, SLOT(data_updated()));

	connect(cursors_.first().get(), SIGNAL(time_changed()),
		this, SLOT(marker_time_changed()));
	connect(cursors_.second().get(), SIGNAL(time_changed()),
		this, SLOT(marker_time_changed()));

	connect(header_, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));

	connect(header_, SIGNAL(selection_changed()),
		cursorheader_, SLOT(clear_selection()));
	connect(cursorheader_, SIGNAL(selection_changed()),
		header_, SLOT(clear_selection()));

	connect(header_, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));
	connect(cursorheader_, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));

	connect(this, SIGNAL(hover_point_changed()),
		this, SLOT(on_hover_point_changed()));

	connect(&lazy_event_handler_, SIGNAL(timeout()),
		this, SLOT(process_sticky_events()));
	lazy_event_handler_.setSingleShot(true);

	setViewport(viewport_);

	viewport_->installEventFilter(this);
	ruler_->installEventFilter(this);
	cursorheader_->installEventFilter(this);
	header_->installEventFilter(this);

	// Trigger the initial event manually. The default device has signals
	// which were created before this object came into being
	signals_changed();

	// make sure the transparent widgets are on the top
	cursorheader_->raise();
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

double View::scale() const
{
	return scale_;
}

double View::offset() const
{
	return offset_;
}

int View::owner_visual_v_offset() const
{
	return -v_offset_;
}

unsigned int View::depth() const
{
	return 0;
}

unsigned int View::tick_prefix() const
{
	return tick_prefix_;
}

double View::tick_period() const
{
	return tick_period_;
}

void View::zoom(double steps)
{
	zoom(steps, viewport_->width() / 2);
}

void View::zoom(double steps, int offset)
{
	set_zoom(scale_ * pow(3.0/2.0, -steps), offset);
}

void View::zoom_fit()
{
	const pair<double, double> extents = get_time_extents();
	const double delta = extents.second - extents.first;
	if (delta < 1e-12)
		return;

	assert(viewport_);
	const int w = viewport_->width();
	if (w <= 0)
		return;

	const double scale = max(min(delta / w, MaxScale), MinScale);
	set_scale_offset(scale, extents.first);
}

void View::zoom_one_to_one()
{
	using pv::data::SignalData;

	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data = get_visible_data();
	if (visible_data.empty())
		return;

	double samplerate = 0.0;
	for (const shared_ptr<SignalData> d : visible_data) {
		assert(d);
		samplerate = max(samplerate, d->samplerate());
	}

	if (samplerate == 0.0)
		return;

	assert(viewport_);
	const int w = viewport_->width();
	if (w <= 0)
		return;

	set_zoom(1.0 / samplerate, w / 2);
}

void View::set_scale_offset(double scale, double offset)
{
	scale_ = scale;
	offset_ = offset;

	calculate_tick_spacing();

	update_scroll();
	ruler_->update();
	cursorheader_->update();
	viewport_->update();
	scale_offset_changed();
}

set< shared_ptr<SignalData> > View::get_visible_data() const
{
	shared_lock<shared_mutex> lock(session().signals_mutex());
	const vector< shared_ptr<Signal> > &sigs(session().signals());

	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data;
	for (const shared_ptr<Signal> sig : sigs)
		if (sig->enabled())
			visible_data.insert(sig->data());

	return visible_data;
}

pair<double, double> View::get_time_extents() const
{
	double left_time = DBL_MAX, right_time = DBL_MIN;
	const set< shared_ptr<SignalData> > visible_data = get_visible_data();
	for (const shared_ptr<SignalData> d : visible_data)
	{
		double samplerate = d->samplerate();
		samplerate = (samplerate <= 0.0) ? 1.0 : samplerate;

		const vector< shared_ptr<Snapshot> > snapshots =
			d->snapshots();
		for (const shared_ptr<Snapshot> &s : snapshots) {
			const double start_time = s->start_time();
			left_time = min(left_time, start_time);
			right_time = max(right_time, start_time +
				d->get_max_sample_count() / samplerate);
		}
	}

	if (left_time == DBL_MAX && right_time == DBL_MIN)
		return make_pair(0.0, 0.0);

	assert(left_time < right_time);
	return make_pair(left_time, right_time);
}

bool View::cursors_shown() const
{
	return show_cursors_;
}

void View::show_cursors(bool show)
{
	show_cursors_ = show;
	cursorheader_->update();
	viewport_->update();
}

void View::centre_cursors()
{
	const double time_width = scale_ * viewport_->width();
	cursors_.first()->set_time(offset_ + time_width * 0.4);
	cursors_.second()->set_time(offset_ + time_width * 0.6);
	cursorheader_->update();
	viewport_->update();
}

CursorPair& View::cursors()
{
	return cursors_;
}

const CursorPair& View::cursors() const
{
	return cursors_;
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

void View::restack_all_row_items()
{
	// Make a set of owners
	unordered_set< RowItemOwner* > owners;
	for (const auto &r : *this)
		owners.insert(r->owner());

	// Make a list that is sorted from deepest first
	vector< RowItemOwner* > sorted_owners(owners.begin(), owners.end());
	sort(sorted_owners.begin(), sorted_owners.end(),
		[](const RowItemOwner* a, const RowItemOwner *b) {
			return a->depth() > b->depth(); });

	// Restack the items recursively
	for (auto &o : sorted_owners)
		o->restack_items();

	// Animate the items to their destination
	for (const auto &r : *this)
		r->animate_to_layout_v_offset();
}

void View::get_scroll_layout(double &length, double &offset) const
{
	const pair<double, double> extents = get_time_extents();
	length = (extents.second - extents.first) / scale_;
	offset = offset_ / scale_;
}

void View::set_zoom(double scale, int offset)
{
	const double cursor_offset = offset_ + scale_ * offset;
	const double new_scale = max(min(scale, MaxScale), MinScale);
	const double new_offset = cursor_offset - new_scale * offset;
	set_scale_offset(new_scale, new_offset);
}

void View::calculate_tick_spacing()
{
	const double SpacingIncrement = 32.0f;
	const double MinValueSpacing = 32.0f;

	double min_width = SpacingIncrement, typical_width;

	QFontMetrics m(QApplication::font());

	do {
		const double min_period = scale_ * min_width;

		const int order = (int)floorf(log10f(min_period));
		const double order_decimal = pow(10.0, order);

		unsigned int unit = 0;

		do {
			tick_period_ = order_decimal * ScaleUnits[unit++];
		} while (tick_period_ < min_period &&
			unit < countof(ScaleUnits));

		tick_prefix_ = (order - pv::util::FirstSIPrefixPower) / 3;

		typical_width = m.boundingRect(0, 0, INT_MAX, INT_MAX,
			Qt::AlignLeft | Qt::AlignTop,
			format_time(offset_, tick_prefix_)).width() +
				MinValueSpacing;

		min_width += SpacingIncrement;

	} while(typical_width > tick_period_ / scale_);
}

void View::update_scroll()
{
	assert(viewport_);

	const QSize areaSize = viewport_->size();

	// Set the horizontal scroll bar
	double length = 0, offset = 0;
	get_scroll_layout(length, offset);
	length = max(length - areaSize.width(), 0.0);

	int major_tick_distance = tick_period_ / scale_;

	horizontalScrollBar()->setPageStep(areaSize.width() / 2);
	horizontalScrollBar()->setSingleStep(major_tick_distance);

	updating_scroll_ = true;

	if (length < MaxScrollValue) {
		horizontalScrollBar()->setRange(0, length);
		horizontalScrollBar()->setSliderPosition(offset);
	} else {
		horizontalScrollBar()->setRange(0, MaxScrollValue);
		horizontalScrollBar()->setSliderPosition(
			offset_ * MaxScrollValue / (scale_ * length));
	}

	updating_scroll_ = false;

	// Set the vertical scrollbar
	verticalScrollBar()->setPageStep(areaSize.height());

	const pair<int, int> extents = v_extents();
	const int extra_scroll_height = (extents.second - extents.first) / 4;
	verticalScrollBar()->setRange(extents.first - extra_scroll_height,
		extents.first + extra_scroll_height);
}

void View::update_layout()
{
	setViewportMargins(
		header_->sizeHint().width() - pv::view::Header::BaselineOffset,
		ruler_->sizeHint().height(), 0, 0);
	ruler_->setGeometry(viewport_->x(), 0,
		viewport_->width(), viewport_->y());
	cursorheader_->setGeometry(
		viewport_->x(),
		ruler_->sizeHint().height() - cursorheader_->sizeHint().height() / 2,
		viewport_->width(), cursorheader_->sizeHint().height());
	header_->setGeometry(0, viewport_->y(),
		header_->sizeHint().width(), viewport_->height());
	update_scroll();
}

void View::paint_label(QPainter &p, int right, bool hover)
{
	(void)p;
	(void)right;
	(void)hover;
}

QRectF View::label_rect(int right)
{
	(void)right;
	return QRectF();
}

bool View::add_channels_to_owner(
	const vector< shared_ptr<sigrok::Channel> > &channels,
	RowItemOwner *owner, int &offset,
	unordered_map<shared_ptr<sigrok::Channel>, shared_ptr<Signal> >
		&signal_map,
	std::function<bool (shared_ptr<RowItem>)> filter_func)
{
	bool any_added = false;

	assert(owner);

	for (const auto &channel : channels)
	{
		const auto iter = signal_map.find(channel);
		if (iter == signal_map.end() ||
			(filter_func && !filter_func((*iter).second)))
			continue;

		shared_ptr<RowItem> row_item = (*iter).second;
		owner->add_child_item(row_item);
		apply_offset(row_item, offset);
		signal_map.erase(iter);

		any_added = true;
	}

	return any_added;
}

void View::apply_offset(shared_ptr<RowItem> row_item, int &offset) {
	assert(row_item);
	const pair<int, int> extents = row_item->v_extents();
	if (row_item->enabled())
		offset += -extents.first;
	row_item->force_to_v_offset(offset);
	if (row_item->enabled())
		offset += extents.second;
}

bool View::eventFilter(QObject *object, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type == QEvent::MouseMove) {

		const QMouseEvent *const mouse_event = (QMouseEvent*)event;
		if (object == viewport_)
			hover_point_ = mouse_event->pos();
		else if (object == ruler_ || object == cursorheader_)
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
	switch(e->type()) {
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

void View::appearance_changed(bool label, bool content)
{
	if (label)
		header_->update();
	if (content)
		viewport_->update();
}

void View::extents_changed(bool horz, bool vert)
{
	sticky_events_ |=
		(horz ? SelectableItemHExtentsChanged : 0) |
		(vert ? SelectableItemVExtentsChanged : 0);
	lazy_event_handler_.start();
}

void View::h_scroll_value_changed(int value)
{
	if (updating_scroll_)
		return;

	const int range = horizontalScrollBar()->maximum();
	if (range < MaxScrollValue)
		offset_ = scale_ * value;
	else {
		double length = 0, offset;
		get_scroll_layout(length, offset);
		offset_ = scale_ * length * value / MaxScrollValue;
	}

	ruler_->update();
	cursorheader_->update();
	viewport_->update();
}

void View::v_scroll_value_changed(int value)
{
	v_offset_ = value;
	header_->update();
	viewport_->update();
}

void View::signals_changed()
{
	int offset = 0;

	// Populate the traces
	clear_child_items();

	shared_ptr<sigrok::Device> device = session_.device();
	assert(device);

	// Collect a set of signals
	unordered_map<shared_ptr<sigrok::Channel>, shared_ptr<Signal> >
		signal_map;

	shared_lock<shared_mutex> lock(session_.signals_mutex());
	const vector< shared_ptr<Signal> > &sigs(session_.signals());

	for (const shared_ptr<Signal> &sig : sigs)
		signal_map[sig->channel()] = sig;

	// Populate channel groups
	for (auto entry : device->channel_groups())
	{
		const shared_ptr<sigrok::ChannelGroup> &group = entry.second;

		if (group->channels().size() <= 1)
			continue;

		shared_ptr<TraceGroup> trace_group(new TraceGroup());
		int child_offset = 0;
		if (add_channels_to_owner(group->channels(),
			trace_group.get(), child_offset, signal_map))
		{
			add_child_item(trace_group);
			apply_offset(trace_group, offset);
		}
	}

	// Add the remaining logic channels
	shared_ptr<TraceGroup> logic_trace_group(new TraceGroup());
	int child_offset = 0;

	if (add_channels_to_owner(device->channels(),
		logic_trace_group.get(), child_offset, signal_map,
		[](shared_ptr<RowItem> r) -> bool {
			return dynamic_pointer_cast<LogicSignal>(r) != nullptr;
			}))

	{
		add_child_item(logic_trace_group);
		apply_offset(logic_trace_group, offset);
	}

	// Add the remaining channels
	add_channels_to_owner(device->channels(), this, offset, signal_map);
	assert(signal_map.empty());

	// Add decode signals
#ifdef ENABLE_DECODE
	const vector< shared_ptr<DecodeTrace> > decode_sigs(
		session().get_decode_signals());
	for (auto s : decode_sigs) {
		add_child_item(s);
		apply_offset(s, offset);
	}
#endif

	update_layout();
}

void View::data_updated()
{
	// Update the scroll bars
	update_scroll();

	// Repaint the view
	viewport_->update();
}

void View::marker_time_changed()
{
	cursorheader_->update();
	viewport_->update();
}

void View::on_signals_moved()
{
	update_scroll();
	signals_moved();
}

void View::process_sticky_events()
{
	if (sticky_events_ & SelectableItemHExtentsChanged)
		update_layout();
	if (sticky_events_ & SelectableItemVExtentsChanged)
		restack_all_row_items();

	// Clear the sticky events
	sticky_events_ = 0;
}

void View::on_hover_point_changed()
{
	for (shared_ptr<RowItem> r : *this)
		r->hover_point_changed();
}

} // namespace view
} // namespace pv
