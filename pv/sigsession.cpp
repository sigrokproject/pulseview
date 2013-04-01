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

#include "sigsession.h"

#include "data/analog.h"
#include "data/analogsnapshot.h"
#include "data/logic.h"
#include "data/logicsnapshot.h"
#include "view/analogsignal.h"
#include "view/logicsignal.h"

#include <assert.h>

#include <QDebug>

using namespace boost;
using namespace std;

namespace pv {

// TODO: This should not be necessary
SigSession* SigSession::_session = NULL;

SigSession::SigSession() :
	_capture_state(Stopped)
{
	// TODO: This should not be necessary
	_session = this;
}

SigSession::~SigSession()
{
	stop_capture();

	if (_sampling_thread.get())
		_sampling_thread->join();
	_sampling_thread.reset();

	// TODO: This should not be necessary
	_session = NULL;
}

void SigSession::load_file(const string &name,
	function<void (const QString)> error_handler)
{
	stop_capture();
	_sampling_thread.reset(new boost::thread(
		&SigSession::load_thread_proc, this, name,
		error_handler));
}

SigSession::capture_state SigSession::get_capture_state() const
{
	lock_guard<mutex> lock(_sampling_mutex);
	return _capture_state;
}

void SigSession::start_capture(struct sr_dev_inst *sdi,
	uint64_t record_length,
	function<void (const QString)> error_handler)
{
	stop_capture();

	// Check that at least one probe is enabled
	const GSList *l;
	for (l = sdi->probes; l; l = l->next) {
		sr_probe *const probe = (sr_probe*)l->data;
		assert(probe);
		if (probe->enabled)
			break;
	}

	if (!l) {
		error_handler(tr("No probes enabled."));
		return;
	}

	// Begin the session
	_sampling_thread.reset(new boost::thread(
		&SigSession::sample_thread_proc, this, sdi,
		record_length, error_handler));
}

void SigSession::stop_capture()
{
	if (get_capture_state() == Stopped)
		return;

	sr_session_stop();

	// Check that sampling stopped
	if (_sampling_thread.get())
		_sampling_thread->join();
	_sampling_thread.reset();
}

vector< shared_ptr<view::Signal> > SigSession::get_signals()
{
	lock_guard<mutex> lock(_signals_mutex);
	return _signals;
}

boost::shared_ptr<data::Logic> SigSession::get_data()
{
	return _logic_data;
}

void SigSession::set_capture_state(capture_state state)
{
	lock_guard<mutex> lock(_sampling_mutex);
	_capture_state = state;
	capture_state_changed(state);
}

void SigSession::load_thread_proc(const string name,
	function<void (const QString)> error_handler)
{
	if (sr_session_load(name.c_str()) != SR_OK) {
		error_handler(tr("Failed to load file."));
		return;
	}

	sr_session_datafeed_callback_add(data_feed_in_proc);

	if (sr_session_start() != SR_OK) {
		error_handler(tr("Failed to start session."));
		return;
	}

	set_capture_state(Running);

	sr_session_run();
	sr_session_stop();

	set_capture_state(Stopped);
}

void SigSession::sample_thread_proc(struct sr_dev_inst *sdi,
	uint64_t record_length,
	function<void (const QString)> error_handler)
{
	assert(sdi);
	assert(error_handler);

	sr_session_new();
	sr_session_datafeed_callback_add(data_feed_in_proc);

	if (sr_session_dev_add(sdi) != SR_OK) {
		error_handler(tr("Failed to use device."));
		sr_session_destroy();
		return;
	}

	// Set the sample limit
	if (sr_config_set(sdi, SR_CONF_LIMIT_SAMPLES,
		g_variant_new_uint64(record_length)) != SR_OK) {
		error_handler(tr("Failed to configure "
			"time-based sample limit."));
		sr_session_destroy();
		return;
	}

	if (sr_session_start() != SR_OK) {
		error_handler(tr("Failed to start session."));
		return;
	}

	set_capture_state(Running);

	sr_session_run();
	sr_session_destroy();

	set_capture_state(Stopped);
}

void SigSession::feed_in_header(const sr_dev_inst *sdi)
{
	shared_ptr<view::Signal> signal;
	GVariant *gvar;
	uint64_t sample_rate = 0;
	unsigned int logic_probe_count = 0;
	unsigned int analog_probe_count = 0;

	// Detect what data types we will receive
	for (const GSList *l = sdi->probes; l; l = l->next) {
		const sr_probe *const probe = (const sr_probe *)l->data;
		if (!probe->enabled)
			continue;

		switch(probe->type) {
		case SR_PROBE_LOGIC:
			logic_probe_count++;
			break;

		case SR_PROBE_ANALOG:
			analog_probe_count++;
			break;
		}
	}

	// Read out the sample rate
	assert(sdi->driver);

	const int ret = sr_config_get(sdi->driver, SR_CONF_SAMPLERATE,
		&gvar, sdi);
	assert(ret == SR_OK);
	sample_rate = g_variant_get_uint64(gvar);
	g_variant_unref(gvar);

	// Create data containers for the coming data snapshots
	{
		lock_guard<mutex> data_lock(_data_mutex);

		if (logic_probe_count != 0) {
			_logic_data.reset(new data::Logic(
				logic_probe_count, sample_rate));
			assert(_logic_data);
		}

		if (analog_probe_count != 0) {
			_analog_data.reset(new data::Analog(sample_rate));
			assert(_analog_data);
		}
	}

	// Make the logic probe list
	{
		lock_guard<mutex> lock(_signals_mutex);

		_signals.clear();

		for (const GSList *l = sdi->probes; l; l = l->next) {
			const sr_probe *const probe =
				(const sr_probe *)l->data;
			assert(probe);
			if (!probe->enabled)
				continue;

			switch(probe->type) {
			case SR_PROBE_LOGIC:
				signal = shared_ptr<view::Signal>(
					new view::LogicSignal(probe->name,
						_logic_data, probe->index));
				break;

			case SR_PROBE_ANALOG:
				signal = shared_ptr<view::Signal>(
					new view::AnalogSignal(probe->name,
						_analog_data, probe->index));
				break;
			}

			_signals.push_back(signal);
		}

		signals_changed();
	}
}

void SigSession::feed_in_meta(const sr_dev_inst *sdi,
	const sr_datafeed_meta &meta)
{
	(void)sdi;

	for (const GSList *l = meta.config; l; l = l->next) {
		const sr_config *const src = (const sr_config*)l->data;
		switch (src->key) {
		case SR_CONF_SAMPLERATE:
			/// @todo handle samplerate changes
			/// samplerate = (uint64_t *)src->value;
			break;
		default:
			// Unknown metadata is not an error.
			break;
		}
	}
}

void SigSession::feed_in_logic(const sr_datafeed_logic &logic)
{
	lock_guard<mutex> lock(_data_mutex);

	if (!_logic_data)
	{
		qDebug() << "Unexpected logic packet";
		return;
	}

	if (!_cur_logic_snapshot)
	{
		// Create a new data snapshot
		_cur_logic_snapshot = shared_ptr<data::LogicSnapshot>(
			new data::LogicSnapshot(logic));
		_logic_data->push_snapshot(_cur_logic_snapshot);
	}
	else
	{
		// Append to the existing data snapshot
		_cur_logic_snapshot->append_payload(logic);
	}

	data_updated();
}

void SigSession::feed_in_analog(const sr_datafeed_analog &analog)
{
	lock_guard<mutex> lock(_data_mutex);

	if(!_analog_data)
	{
		qDebug() << "Unexpected analog packet";
		return;	// This analog packet was not expected.
	}

	if (!_cur_analog_snapshot)
	{
		// Create a new data snapshot
		_cur_analog_snapshot = shared_ptr<data::AnalogSnapshot>(
			new data::AnalogSnapshot(analog));
		_analog_data->push_snapshot(_cur_analog_snapshot);
	}
	else
	{
		// Append to the existing data snapshot
		_cur_analog_snapshot->append_payload(analog);
	}

	data_updated();
}

void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
	const struct sr_datafeed_packet *packet)
{
	assert(sdi);
	assert(packet);

	switch (packet->type) {
	case SR_DF_HEADER:
		feed_in_header(sdi);
		break;

	case SR_DF_META:
		assert(packet->payload);
		feed_in_meta(sdi,
			*(const sr_datafeed_meta*)packet->payload);
		break;

	case SR_DF_LOGIC:
		assert(packet->payload);
		feed_in_logic(*(const sr_datafeed_logic*)packet->payload);
		break;

	case SR_DF_ANALOG:
		assert(packet->payload);
		feed_in_analog(*(const sr_datafeed_analog*)packet->payload);
		break;

	case SR_DF_END:
	{
		{
			lock_guard<mutex> lock(_data_mutex);
			_cur_logic_snapshot.reset();
			_cur_analog_snapshot.reset();
		}
		data_updated();
		break;
	}
	}
}

void SigSession::data_feed_in_proc(const struct sr_dev_inst *sdi,
	const struct sr_datafeed_packet *packet)
{
	assert(_session);
	_session->data_feed_in(sdi, packet);
}

} // namespace pv
