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

#include <extdef.h>

#include <cassert>
#include <cmath>

#include <algorithm>

#include <QFormLayout>
#include <QToolBar>

#include "logicsignal.h"
#include "view.h"

#include <pv/sigsession.h>
#include <pv/devicemanager.h>
#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/view/view.h>

#include <libsigrok/libsigrok.hpp>

using std::deque;
using std::max;
using std::make_pair;
using std::min;
using std::pair;
using std::shared_ptr;
using std::vector;

using sigrok::Channel;
using sigrok::ConfigKey;
using sigrok::Device;
using sigrok::Error;
using sigrok::Trigger;
using sigrok::TriggerStage;
using sigrok::TriggerMatch;
using sigrok::TriggerMatchType;

namespace pv {
namespace view {

const int LogicSignal::SignalHeight = 30;
const int LogicSignal::SignalMargin = 10;

const float LogicSignal::Oversampling = 2.0f;

const QColor LogicSignal::EdgeColour(0x80, 0x80, 0x80);
const QColor LogicSignal::HighColour(0x00, 0xC0, 0x00);
const QColor LogicSignal::LowColour(0xC0, 0x00, 0x00);

const QColor LogicSignal::SignalColours[10] = {
	QColor(0x16, 0x19, 0x1A),	// Black
	QColor(0x8F, 0x52, 0x02),	// Brown
	QColor(0xCC, 0x00, 0x00),	// Red
	QColor(0xF5, 0x79, 0x00),	// Orange
	QColor(0xED, 0xD4, 0x00),	// Yellow
	QColor(0x73, 0xD2, 0x16),	// Green
	QColor(0x34, 0x65, 0xA4),	// Blue
	QColor(0x75, 0x50, 0x7B),	// Violet
	QColor(0x88, 0x8A, 0x85),	// Grey
	QColor(0xEE, 0xEE, 0xEC),	// White
};

LogicSignal::LogicSignal(
	pv::SigSession &session,
	shared_ptr<Device> device,
	shared_ptr<Channel> channel,
	shared_ptr<data::Logic> data) :
	Signal(session, channel),
	_device(device),
	_data(data),
	_trigger_none(NULL),
	_trigger_rising(NULL),
	_trigger_high(NULL),
	_trigger_falling(NULL),
	_trigger_low(NULL),
	_trigger_change(NULL)
{
	shared_ptr<Trigger> trigger;

	_colour = SignalColours[channel->index() % countof(SignalColours)];

	/* Populate this channel's trigger setting with whatever we
	 * find in the current session trigger, if anything. */
	_trigger_match = nullptr;
	if ((trigger = _session.session()->trigger()))
		for (auto stage : trigger->stages())
			for (auto match : stage->matches())
				if (match->channel() == _channel)
					_trigger_match = match->type();
}

LogicSignal::~LogicSignal()
{
}

shared_ptr<pv::data::SignalData> LogicSignal::data() const
{
	return _data;
}

shared_ptr<pv::data::Logic> LogicSignal::logic_data() const
{
	return _data;
}

std::pair<int, int> LogicSignal::v_extents() const
{
	return make_pair(-SignalHeight - SignalMargin, SignalMargin);
}

void LogicSignal::paint_back(QPainter &p, int left, int right)
{
	if (_channel->enabled())
		paint_axis(p, get_y(), left, right);
}

void LogicSignal::paint_mid(QPainter &p, int left, int right)
{
	using pv::view::View;

	QLineF *line;

	vector< pair<int64_t, bool> > edges;

	assert(_channel);
	assert(_data);
	assert(right >= left);
	assert(_owner);

	const int y = get_y();

	const View *const view = _owner->view();
	assert(view);
	
	const double scale = view->scale();
	assert(scale > 0);

	const double offset = view->offset();

	if (!_channel->enabled())
		return;

	const float high_offset = y - SignalHeight + 0.5f;
	const float low_offset = y + 0.5f;

	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();

	double samplerate = _data->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = offset / scale;
	const double start_time = _data->get_start_time();
	const int64_t last_sample = snapshot->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * scale;
	const double start = samplerate * (offset - start_time);
	const double end = start + samples_per_pixel * (right - left);

	snapshot->get_subsampled_edges(edges,
		min(max((int64_t)floor(start), (int64_t)0), last_sample),
		min(max((int64_t)ceil(end), (int64_t)0), last_sample),
		samples_per_pixel / Oversampling, _channel->index());
	assert(edges.size() >= 2);

	// Paint the edges
	const unsigned int edge_count = edges.size() - 2;
	QLineF *const edge_lines = new QLineF[edge_count];
	line = edge_lines;

	for (auto i = edges.cbegin() + 1; i != edges.cend() - 1; i++) {
		const float x = ((*i).first / samples_per_pixel -
			pixels_offset) + left;
		*line++ = QLineF(x, high_offset, x, low_offset);
	}

	p.setPen(EdgeColour);
	p.drawLines(edge_lines, edge_count);
	delete[] edge_lines;

	// Paint the caps
	const unsigned int max_cap_line_count = edges.size();
	QLineF *const cap_lines = new QLineF[max_cap_line_count];

	p.setPen(HighColour);
	paint_caps(p, cap_lines, edges, true, samples_per_pixel,
		pixels_offset, left, high_offset);
	p.setPen(LowColour);
	paint_caps(p, cap_lines, edges, false, samples_per_pixel,
		pixels_offset, left, low_offset);

	delete[] cap_lines;
}

void LogicSignal::paint_caps(QPainter &p, QLineF *const lines,
	vector< pair<int64_t, bool> > &edges, bool level,
	double samples_per_pixel, double pixels_offset, float x_offset,
	float y_offset)
{
	QLineF *line = lines;

	for (auto i = edges.begin(); i != (edges.end() - 1); i++)
		if ((*i).second == level) {
			*line++ = QLineF(
				((*i).first / samples_per_pixel -
					pixels_offset) + x_offset, y_offset,
				((*(i+1)).first / samples_per_pixel -
					pixels_offset) + x_offset, y_offset);
		}

	p.drawLines(lines, line - lines);
}

void LogicSignal::init_trigger_actions(QWidget *parent)
{
	_trigger_none = new QAction(QIcon(":/icons/trigger-none.svg"),
		tr("No trigger"), parent);
	_trigger_none->setCheckable(true);
	connect(_trigger_none, SIGNAL(triggered()), this, SLOT(on_trigger()));

	_trigger_rising = new QAction(QIcon(":/icons/trigger-rising.svg"),
		tr("Trigger on rising edge"), parent);
	_trigger_rising->setCheckable(true);
	connect(_trigger_rising, SIGNAL(triggered()), this, SLOT(on_trigger()));

	_trigger_high = new QAction(QIcon(":/icons/trigger-high.svg"),
		tr("Trigger on high level"), parent);
	_trigger_high->setCheckable(true);
	connect(_trigger_high, SIGNAL(triggered()), this, SLOT(on_trigger()));

	_trigger_falling = new QAction(QIcon(":/icons/trigger-falling.svg"),
		tr("Trigger on falling edge"), parent);
	_trigger_falling->setCheckable(true);
	connect(_trigger_falling, SIGNAL(triggered()), this, SLOT(on_trigger()));

	_trigger_low = new QAction(QIcon(":/icons/trigger-low.svg"),
		tr("Trigger on low level"), parent);
	_trigger_low->setCheckable(true);
	connect(_trigger_low, SIGNAL(triggered()), this, SLOT(on_trigger()));

	_trigger_change = new QAction(QIcon(":/icons/trigger-change.svg"),
		tr("Trigger on rising or falling edge"), parent);
	_trigger_change->setCheckable(true);
	connect(_trigger_change, SIGNAL(triggered()), this, SLOT(on_trigger()));
}

QAction* LogicSignal::match_action(const TriggerMatchType *type)
{
	QAction *action;

	action = _trigger_none;
	if (type) {
		switch (type->id()) {
		case SR_TRIGGER_ZERO:
			action = _trigger_low;
			break;
		case SR_TRIGGER_ONE:
			action = _trigger_high;
			break;
		case SR_TRIGGER_RISING:
			action = _trigger_rising;
			break;
		case SR_TRIGGER_FALLING:
			action = _trigger_falling;
			break;
		case SR_TRIGGER_EDGE:
			action = _trigger_change;
			break;
		default:
			assert(0);
		}
	}

	return action;
}

const TriggerMatchType *LogicSignal::action_match(QAction *action)
{
	if (action == _trigger_low)
		return TriggerMatchType::ZERO;
	else if (action == _trigger_high)
		return TriggerMatchType::ONE;
	else if (action == _trigger_rising)
		return TriggerMatchType::RISING;
	else if (action == _trigger_falling)
		return TriggerMatchType::FALLING;
	else if (action == _trigger_change)
		return TriggerMatchType::EDGE;
	else
		return nullptr;
}

void LogicSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	Glib::VariantContainerBase gvar;
	vector<int32_t> trig_types;

	Signal::populate_popup_form(parent, form);

	try {
		gvar = _device->config_list(ConfigKey::TRIGGER_MATCH);
	} catch (Error e) {
		return;
	}

	_trigger_bar = new QToolBar(parent);
	init_trigger_actions(_trigger_bar);
	_trigger_bar->addAction(_trigger_none);
	_trigger_none->setChecked(!_trigger_match);
	trig_types =
		Glib::VariantBase::cast_dynamic<Glib::Variant<vector<int32_t>>>(
			gvar).get();
	for (auto type_id : trig_types) {
		auto type = TriggerMatchType::get(type_id);
		_trigger_bar->addAction(match_action(type));
		match_action(type)->setChecked(_trigger_match == type);
	}
	form->addRow(tr("Trigger"), _trigger_bar);

}

void LogicSignal::modify_trigger()
{
	auto trigger = _session.session()->trigger();
	auto new_trigger = _session.device_manager().context()->create_trigger("pulseview");

	if (trigger) {
		for (auto stage : trigger->stages()) {
			const auto &matches = stage->matches();
			if (std::none_of(begin(matches), end(matches),
			    [&](shared_ptr<TriggerMatch> match) {
					return match->channel() != _channel; }))
				continue;

			auto new_stage = new_trigger->add_stage();
			for (auto match : stage->matches()) {
				if (match->channel() == _channel)
					continue;
				new_stage->add_match(match->channel(), match->type());
			}
		}
	}

	if (_trigger_match)
		new_trigger->add_stage()->add_match(_channel, _trigger_match);

	_session.session()->set_trigger(
		new_trigger->stages().empty() ? nullptr : new_trigger);
}

void LogicSignal::on_trigger()
{
	QAction *action;

	match_action(_trigger_match)->setChecked(false);

	action = (QAction *)sender();
	action->setChecked(true);
	_trigger_match = action_match(action);

	modify_trigger();
}

} // namespace view
} // namespace pv
