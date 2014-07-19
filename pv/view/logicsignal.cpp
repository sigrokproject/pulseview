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

#include <QFormLayout>
#include <QToolBar>

#include "logicsignal.h"
#include "view.h"

#include <pv/sigsession.h>
#include <pv/device/devinst.h>
#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/view/view.h>

using std::deque;
using std::max;
using std::min;
using std::pair;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

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

LogicSignal::LogicSignal(shared_ptr<pv::device::DevInst> dev_inst,
	const sr_channel *const probe, shared_ptr<data::Logic> data) :
	Signal(dev_inst, probe),
	_data(data),
	_trigger_none(NULL),
	_trigger_rising(NULL),
	_trigger_high(NULL),
	_trigger_falling(NULL),
	_trigger_low(NULL),
	_trigger_change(NULL)
{
	struct sr_trigger *trigger;
	struct sr_trigger_stage *stage;
	struct sr_trigger_match *match;
	const GSList *l, *m;

	_colour = SignalColours[probe->index % countof(SignalColours)];

	/* Populate this channel's trigger setting with whatever we
	 * find in the current session trigger, if anything. */
	_trigger_match = 0;
	if ((trigger = sr_session_trigger_get(SigSession::_sr_session))) {
		for (l = trigger->stages; l && !_trigger_match; l = l->next) {
			stage = (struct sr_trigger_stage *)l->data;
			for (m = stage->matches; m && !_trigger_match; m = m->next) {
				match = (struct sr_trigger_match *)m->data;
				if (match->channel == _probe)
					_trigger_match = match->match;
			}
		}
	}
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

void LogicSignal::paint_back(QPainter &p, int left, int right)
{
	if (_probe->enabled)
		paint_axis(p, get_y(), left, right);
}

void LogicSignal::paint_mid(QPainter &p, int left, int right)
{
	using pv::view::View;

	QLineF *line;

	vector< pair<int64_t, bool> > edges;

	assert(_probe);
	assert(_data);
	assert(right >= left);

	assert(_view);
	const int y = _v_offset - _view->v_offset();
	
	const double scale = _view->scale();
	assert(scale > 0);
	
	const double offset = _view->offset();

	if (!_probe->enabled)
		return;

	const float high_offset = y - View::SignalHeight + 0.5f;
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
		samples_per_pixel / Oversampling, _probe->index);
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

QAction* LogicSignal::match_action(int match)
{
	QAction *action;

	action = _trigger_none;
	switch (match) {
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
	}

	return action;
}

int LogicSignal::action_match(QAction *action)
{
	int match;

	if (action == _trigger_low)
		match = SR_TRIGGER_ZERO;
	else if (action == _trigger_high)
		match = SR_TRIGGER_ONE;
	else if (action == _trigger_rising)
		match = SR_TRIGGER_RISING;
	else if (action == _trigger_falling)
		match = SR_TRIGGER_FALLING;
	else if (action == _trigger_change)
		match = SR_TRIGGER_EDGE;
	else
		match = 0;

	return match;
}

void LogicSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	GVariant *gvar;
	gsize num_opts;
	const int32_t *trig_matches;
	unsigned int i;
	bool is_checked;

	Signal::populate_popup_form(parent, form);

	if (!(gvar = _dev_inst->list_config(NULL, SR_CONF_TRIGGER_MATCH)))
		return;

	_trigger_bar = new QToolBar(parent);
	init_trigger_actions(_trigger_bar);
	_trigger_bar->addAction(_trigger_none);
	trig_matches = (const int32_t *)g_variant_get_fixed_array(gvar,
			&num_opts, sizeof(int32_t));
	for (i = 0; i < num_opts; i++) {
		_trigger_bar->addAction(match_action(trig_matches[i]));
		is_checked = _trigger_match == trig_matches[i];
		match_action(trig_matches[i])->setChecked(is_checked);
	}
	form->addRow(tr("Trigger"), _trigger_bar);
	g_variant_unref(gvar);

}

void LogicSignal::on_trigger()
{
	QAction *action;

	match_action(_trigger_match)->setChecked(FALSE);

	action = (QAction *)sender();
	action->setChecked(TRUE);
	_trigger_match = action_match(action);

}

} // namespace view
} // namespace pv
