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

#include "logicsignal.hpp"
#include "view.hpp"

#include <pv/session.hpp>
#include <pv/devicemanager.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsnapshot.hpp>
#include <pv/view/view.hpp>

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

QColor LogicSignal::TriggerMarkerBackgroundColour = QColor(0xED, 0xD4, 0x00);
const int LogicSignal::TriggerMarkerPadding = 2;
const char* LogicSignal::TriggerMarkerIcons[8] = {
	nullptr,
	":/icons/trigger-marker-low.svg",
	":/icons/trigger-marker-high.svg",
	":/icons/trigger-marker-rising.svg",
	":/icons/trigger-marker-falling.svg",
	":/icons/trigger-marker-change.svg",
	nullptr,
	nullptr
};

QCache<QString, const QIcon> LogicSignal::icon_cache_;
QCache<QString, const QPixmap> LogicSignal::pixmap_cache_;

LogicSignal::LogicSignal(
	pv::Session &session,
	shared_ptr<Device> device,
	shared_ptr<Channel> channel,
	shared_ptr<data::Logic> data) :
	Signal(session, channel),
	device_(device),
	data_(data),
	trigger_none_(NULL),
	trigger_rising_(NULL),
	trigger_high_(NULL),
	trigger_falling_(NULL),
	trigger_low_(NULL),
	trigger_change_(NULL)
{
	shared_ptr<Trigger> trigger;

	colour_ = SignalColours[channel->index() % countof(SignalColours)];

	/* Populate this channel's trigger setting with whatever we
	 * find in the current session trigger, if anything. */
	trigger_match_ = nullptr;
	if ((trigger = session_.session()->trigger()))
		for (auto stage : trigger->stages())
			for (auto match : stage->matches())
				if (match->channel() == channel_)
					trigger_match_ = match->type();
}

LogicSignal::~LogicSignal()
{
}

shared_ptr<pv::data::SignalData> LogicSignal::data() const
{
	return data_;
}

shared_ptr<pv::data::Logic> LogicSignal::logic_data() const
{
	return data_;
}

std::pair<int, int> LogicSignal::v_extents() const
{
	return make_pair(-SignalHeight - SignalMargin, SignalMargin);
}

void LogicSignal::paint_back(QPainter &p, int left, int right)
{
	if (channel_->enabled())
		paint_axis(p, get_visual_y(), left, right);
}

void LogicSignal::paint_mid(QPainter &p, int left, int right)
{
	using pv::view::View;

	QLineF *line;

	vector< pair<int64_t, bool> > edges;

	assert(channel_);
	assert(data_);
	assert(right >= left);
	assert(owner_);

	const int y = get_visual_y();

	const View *const view = owner_->view();
	assert(view);

	const double scale = view->scale();
	assert(scale > 0);

	const double offset = view->offset();

	if (!channel_->enabled())
		return;

	const float high_offset = y - SignalHeight + 0.5f;
	const float low_offset = y + 0.5f;

	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		data_->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();

	double samplerate = data_->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = offset / scale;
	const double start_time = data_->get_start_time();
	const int64_t last_sample = snapshot->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * scale;
	const double start = samplerate * (offset - start_time);
	const double end = start + samples_per_pixel * (right - left);

	snapshot->get_subsampled_edges(edges,
		min(max((int64_t)floor(start), (int64_t)0), last_sample),
		min(max((int64_t)ceil(end), (int64_t)0), last_sample),
		samples_per_pixel / Oversampling, channel_->index());
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

void LogicSignal::paint_fore(QPainter &p, int left, int right)
{
	(void)left;

	// Draw the trigger marker
	if (!trigger_match_)
		return;

	const int y = get_visual_y();
	const vector<int32_t> trig_types = get_trigger_types();
	for (int32_t type_id : trig_types) {
		const TriggerMatchType *const type =
			TriggerMatchType::get(type_id);
		if (trigger_match_ != type || type_id < 0 ||
			(size_t)type_id >= countof(TriggerMarkerIcons) ||
			!TriggerMarkerIcons[type_id])
			continue;

		const QPixmap *const pixmap = get_pixmap(
			TriggerMarkerIcons[type_id]);
		if (!pixmap)
			continue;

		const int pad = TriggerMarkerPadding;
		const QSize size = pixmap->size();
		const QPoint point(
			right - size.width() - pad * 2,
			y - (SignalHeight + size.height()) / 2);

		p.setPen(QPen(Qt::NoPen));
		p.setBrush(TriggerMarkerBackgroundColour);
		p.drawRoundedRect(QRect(point, size).adjusted(
			-pad, -pad, pad, pad), pad, pad);
		p.drawPixmap(point, *pixmap);

		break;
	}
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
	trigger_none_ = new QAction(*get_icon(":/icons/trigger-none.svg"),
		tr("No trigger"), parent);
	trigger_none_->setCheckable(true);
	connect(trigger_none_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_rising_ = new QAction(*get_icon(":/icons/trigger-rising.svg"),
		tr("Trigger on rising edge"), parent);
	trigger_rising_->setCheckable(true);
	connect(trigger_rising_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_high_ = new QAction(*get_icon(":/icons/trigger-high.svg"),
		tr("Trigger on high level"), parent);
	trigger_high_->setCheckable(true);
	connect(trigger_high_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_falling_ = new QAction(*get_icon(":/icons/trigger-falling.svg"),
		tr("Trigger on falling edge"), parent);
	trigger_falling_->setCheckable(true);
	connect(trigger_falling_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_low_ = new QAction(*get_icon(":/icons/trigger-low.svg"),
		tr("Trigger on low level"), parent);
	trigger_low_->setCheckable(true);
	connect(trigger_low_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_change_ = new QAction(*get_icon(":/icons/trigger-change.svg"),
		tr("Trigger on rising or falling edge"), parent);
	trigger_change_->setCheckable(true);
	connect(trigger_change_, SIGNAL(triggered()), this, SLOT(on_trigger()));
}

const vector<int32_t> LogicSignal::get_trigger_types() const
{
	try {
		const Glib::VariantContainerBase gvar =
			device_->config_list(ConfigKey::TRIGGER_MATCH);
		return Glib::VariantBase::cast_dynamic<
			Glib::Variant<vector<int32_t>>>(gvar).get();
	} catch (Error e) {
		return vector<int32_t>();
	}
}

QAction* LogicSignal::action_from_trigger_type(const TriggerMatchType *type)
{
	QAction *action;

	action = trigger_none_;
	if (type) {
		switch (type->id()) {
		case SR_TRIGGER_ZERO:
			action = trigger_low_;
			break;
		case SR_TRIGGER_ONE:
			action = trigger_high_;
			break;
		case SR_TRIGGER_RISING:
			action = trigger_rising_;
			break;
		case SR_TRIGGER_FALLING:
			action = trigger_falling_;
			break;
		case SR_TRIGGER_EDGE:
			action = trigger_change_;
			break;
		default:
			assert(0);
		}
	}

	return action;
}

const TriggerMatchType *LogicSignal::trigger_type_from_action(QAction *action)
{
	if (action == trigger_low_)
		return TriggerMatchType::ZERO;
	else if (action == trigger_high_)
		return TriggerMatchType::ONE;
	else if (action == trigger_rising_)
		return TriggerMatchType::RISING;
	else if (action == trigger_falling_)
		return TriggerMatchType::FALLING;
	else if (action == trigger_change_)
		return TriggerMatchType::EDGE;
	else
		return nullptr;
}

void LogicSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	Signal::populate_popup_form(parent, form);

	trigger_bar_ = new QToolBar(parent);
	init_trigger_actions(trigger_bar_);
	trigger_bar_->addAction(trigger_none_);
	trigger_none_->setChecked(!trigger_match_);

	const vector<int32_t> trig_types = get_trigger_types();
	for (auto type_id : trig_types) {
		const TriggerMatchType *const type =
			TriggerMatchType::get(type_id);
		QAction *const action = action_from_trigger_type(type);
		trigger_bar_->addAction(action);
		action->setChecked(trigger_match_ == type);
	}
	form->addRow(tr("Trigger"), trigger_bar_);

}

void LogicSignal::modify_trigger()
{
	auto trigger = session_.session()->trigger();
	auto new_trigger = session_.device_manager().context()->create_trigger("pulseview");

	if (trigger) {
		for (auto stage : trigger->stages()) {
			const auto &matches = stage->matches();
			if (std::none_of(begin(matches), end(matches),
			    [&](shared_ptr<TriggerMatch> match) {
					return match->channel() != channel_; }))
				continue;

			auto new_stage = new_trigger->add_stage();
			for (auto match : stage->matches()) {
				if (match->channel() == channel_)
					continue;
				new_stage->add_match(match->channel(), match->type());
			}
		}
	}

	if (trigger_match_)
		new_trigger->add_stage()->add_match(channel_, trigger_match_);

	session_.session()->set_trigger(
		new_trigger->stages().empty() ? nullptr : new_trigger);

	if (owner_)
		owner_->appearance_changed(false, true);
}

const QIcon* LogicSignal::get_icon(const char *path)
{
	const QIcon *icon = icon_cache_.take(path);
	if (!icon) {
		icon = new QIcon(path);
		icon_cache_.insert(path, icon);
	}

	return icon;
}

const QPixmap* LogicSignal::get_pixmap(const char *path)
{
	const QPixmap *pixmap = pixmap_cache_.take(path);
	if (!pixmap) {
		pixmap = new QPixmap(path);
		pixmap_cache_.insert(path, pixmap);
	}

	return pixmap;
}

void LogicSignal::on_trigger()
{
	QAction *action;

	action_from_trigger_type(trigger_match_)->setChecked(false);

	action = (QAction *)sender();
	action->setChecked(true);
	trigger_match_ = trigger_type_from_action(action);

	modify_trigger();
}

} // namespace view
} // namespace pv
