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

#include <math.h>

#include "logicsignal.h"
#include "view.h"

#include "pv/sigsession.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"

using namespace boost;
using namespace std;

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

LogicSignal::LogicSignal(pv::SigSession &session, const sr_probe *const probe,
	shared_ptr<data::Logic> data) :
	Signal(session, probe),
	_data(data),
	_separator(this),
	_icon_trigger_none(":/icons/trigger-none.svg"),
	_trigger_none(_icon_trigger_none, tr("No trigger"), this),
	_icon_trigger_rising(":/icons/trigger-rising.svg"),
	_trigger_rising(_icon_trigger_rising,
		tr("Trigger on rising edge"), this),
	_icon_trigger_high(":/icons/trigger-high.svg"),
	_trigger_high(_icon_trigger_high,
		tr("Trigger on high level"), this),
	_icon_trigger_falling(":/icons/trigger-falling.svg"),
	_trigger_falling(_icon_trigger_falling,
		tr("Trigger on falling edge"), this),
	_icon_trigger_low(":/icons/trigger-low.svg"),
	_trigger_low(_icon_trigger_low,
		tr("Trigger on low level"), this),
	_icon_trigger_change(":/icons/trigger-change.svg"),
	_trigger_change(_icon_trigger_change,
		tr("Trigger on rising or falling edge"), this)
{
	_colour = SignalColours[probe->index % countof(SignalColours)];

	_separator.setSeparator(true);

	_trigger_none.setCheckable(true);
	connect(&_trigger_none, SIGNAL(triggered()),
		this, SLOT(on_trigger_none()));

	_trigger_rising.setCheckable(true);
	connect(&_trigger_rising, SIGNAL(triggered()),
		this, SLOT(on_trigger_rising()));

	_trigger_high.setCheckable(true);
	connect(&_trigger_high, SIGNAL(triggered()),
		this, SLOT(on_trigger_high()));

	_trigger_falling.setCheckable(true);
	connect(&_trigger_falling, SIGNAL(triggered()),
		this, SLOT(on_trigger_falling()));

	_trigger_low.setCheckable(true);
	connect(&_trigger_low, SIGNAL(triggered()),
		this, SLOT(on_trigger_low()));

	_trigger_change.setCheckable(true);
	connect(&_trigger_change, SIGNAL(triggered()),
		this, SLOT(on_trigger_change()));
}

LogicSignal::~LogicSignal()
{
}

const list<QAction*> LogicSignal::get_context_bar_actions()
{
	GVariant *gvar;
	list<QAction*> actions;

	actions.push_back(&_name_action);

	// Add the trigger actions
	const sr_dev_inst *const sdi = _session.get_device();
	if (sr_config_list(sdi->driver, SR_CONF_TRIGGER_TYPE,
		&gvar, sdi) == SR_OK) {
		const char *const trig_types =
			g_variant_get_string(gvar, NULL);

		if (trig_types && trig_types[0] != '\0') {
			actions.push_back(&_separator);

			actions.push_back(&_trigger_none);

			add_trigger_action(trig_types, 'r',
				&_trigger_rising, actions);
			add_trigger_action(trig_types, '1',
				&_trigger_high, actions);
			add_trigger_action(trig_types, 'f',
				&_trigger_falling, actions);
			add_trigger_action(trig_types, '0',
				&_trigger_low, actions);
			add_trigger_action(trig_types, 'c',
				&_trigger_change, actions);
		
			update_trigger_actions();
		}

		g_variant_unref(gvar);
	}

	return actions;
}

void LogicSignal::paint(QPainter &p, int y, int left, int right,
		double scale, double offset)
{
	using pv::view::View;

	QLineF *line;

	vector< pair<int64_t, bool> > edges;

	assert(_probe);
	assert(scale > 0);
	assert(_data);
	assert(right >= left);

	if (!_probe->enabled)
		return;

	paint_axis(p, y, left, right);

	const float high_offset = y - View::SignalHeight + 0.5f;
	const float low_offset = y + 0.5f;

	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();

	double samplerate = _data->get_samplerate();

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

	for (vector<pv::data::LogicSnapshot::EdgePair>::const_iterator i =
			edges.begin() + 1;
		i != edges.end() - 1; i++) {
		const float x = ((*i).first / samples_per_pixel -
			pixels_offset) + left;
		*line++ = QLineF(x, high_offset, x, low_offset);
	}

	p.setPen(EdgeColour);
	p.drawLines(edge_lines, edge_count);
	delete[] edge_lines;

	// Paint the caps
	const unsigned int max_cap_line_count = (edges.size() - 1);
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

	for (vector<pv::data::LogicSnapshot::EdgePair>::const_iterator i =
		edges.begin(); i != (edges.end() - 1); i++)
		if ((*i).second == level) {
			*line++ = QLineF(
				((*i).first / samples_per_pixel -
					pixels_offset) + x_offset, y_offset,
				((*(i+1)).first / samples_per_pixel -
					pixels_offset) + x_offset, y_offset);
		}

	p.drawLines(lines, line - lines);
}

void LogicSignal::add_trigger_action(const char *trig_types, char type,
	QAction *action, list<QAction*> &actions)
{
	while(*trig_types)
		if(*trig_types++ == type) {
			actions.push_back(action);
			break;
		}
}

void LogicSignal::update_trigger_actions()
{
	const char cur_trigger = _probe->trigger ?
		_probe->trigger[0] : '\0';
	_trigger_none.setChecked(cur_trigger == '\0');
	_trigger_rising.setChecked(cur_trigger == 'r');
	_trigger_high.setChecked(cur_trigger == '1');
	_trigger_falling.setChecked(cur_trigger == 'f');
	_trigger_low.setChecked(cur_trigger == '0');
	_trigger_change.setChecked(cur_trigger == 'c');
}

void LogicSignal::set_trigger(char type)
{
	const char trigger_type_string[2] = {type, 0};
	const char *const trigger_string =
		(type != 0) ? trigger_type_string : NULL;

	const sr_dev_inst *const sdi = _session.get_device();
	const int probe_count = g_slist_length(sdi->probes);
	assert(probe_count > 0);

	assert(_probe && _probe->index < probe_count);

	for (int i = 0; i < probe_count; i++) {
		sr_dev_trigger_set(sdi, i, (i == _probe->index) ?
			trigger_string : NULL);
	}

	update_trigger_actions();
}

void LogicSignal::on_trigger_none()
{
	set_trigger('\0');	
}

void LogicSignal::on_trigger_rising()
{
	set_trigger('r');	
}

void LogicSignal::on_trigger_high()
{
	set_trigger('1');	
}

void LogicSignal::on_trigger_falling()
{
	set_trigger('f');	
}

void LogicSignal::on_trigger_low()
{
	set_trigger('0');	
}

void LogicSignal::on_trigger_change()
{
	set_trigger('c');	
}

} // namespace view
} // namespace pv
