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

#include "logicdata.h"
#include "logicdatasnapshot.h"
#include "view/logicsignal.h"

#include <QDebug>

#include <assert.h>

using namespace boost;
using namespace std;

namespace pv {

// TODO: This should not be necessary
SigSession* SigSession::_session = NULL;

SigSession::SigSession()
{
	// TODO: This should not be necessary
	_session = this;
}

SigSession::~SigSession()
{
	if(_sampling_thread.get())
		_sampling_thread->join();
	_sampling_thread.reset();

	// TODO: This should not be necessary
	_session = NULL;
}

void SigSession::load_file(const std::string &name)
{
	if (sr_session_load(name.c_str()) == SR_OK) {
		/* sigrok session file */
		sr_session_datafeed_callback_add(data_feed_in_proc);
		sr_session_start();
		sr_session_run();
		sr_session_stop();
	}
}

void SigSession::start_capture(struct sr_dev_inst *sdi,
	uint64_t record_length, uint64_t sample_rate)
{
	// Check sampling isn't already active
	if(_sampling_thread.get())
		_sampling_thread->join();

	_sampling_thread.reset(new boost::thread(
		&SigSession::sample_thread_proc, this, sdi,
		record_length, sample_rate));
}

vector< shared_ptr<view::Signal> > SigSession::get_signals()
{
	lock_guard<mutex> lock(_signals_mutex);
	return _signals;
}

boost::shared_ptr<LogicData> SigSession::get_data()
{
	return _logic_data;
}

void SigSession::sample_thread_proc(struct sr_dev_inst *sdi,
	uint64_t record_length, uint64_t sample_rate)
{
	sr_session_new();
	sr_session_datafeed_callback_add(data_feed_in_proc);

	if (sr_session_dev_add(sdi) != SR_OK) {
		qDebug() << "Failed to use device.";
		sr_session_destroy();
		return;
	}

	if (sr_dev_config_set(sdi, SR_HWCAP_LIMIT_SAMPLES,
		&record_length) != SR_OK) {
		qDebug() << "Failed to configure time-based sample limit.";
		sr_session_destroy();
		return;
	}

	if (sr_dev_config_set(sdi, SR_HWCAP_SAMPLERATE,
		&sample_rate) != SR_OK) {
		qDebug() << "Failed to configure samplerate.";
		sr_session_destroy();
		return;
	}

	if (sr_session_start() != SR_OK) {
		qDebug() << "Failed to start session.";
		return;
	}

	sr_session_run();
	sr_session_destroy();
}

void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
	struct sr_datafeed_packet *packet)
{
	using view::LogicSignal;

	assert(sdi);
	assert(packet);

	switch (packet->type) {
	case SR_DF_HEADER:
	{
		lock_guard<mutex> lock(_signals_mutex);
		_signals.clear();
		break;
	}

	case SR_DF_META_LOGIC:
	{
		assert(packet->payload);
		const sr_datafeed_meta_logic &meta_logic =
			*(sr_datafeed_meta_logic*)packet->payload;

	{
		lock_guard<mutex> lock(_data_mutex);

		// Create an empty LogiData for coming data snapshots
		_logic_data.reset(new LogicData(meta_logic));
		assert(_logic_data);
		if(!_logic_data)
			break;
	}

	{
		lock_guard<mutex> lock(_signals_mutex);

		// Add the signals
		for (int i = 0; i < meta_logic.num_probes; i++)
		{
			const sr_probe *const probe =
				(const sr_probe*)g_slist_nth_data(
					sdi->probes, i);
			if(probe->enabled)
			{
				shared_ptr<LogicSignal> signal(
					new LogicSignal(probe->name,
						_logic_data,
						probe->index));
				_signals.push_back(signal);
			}
		}

		signals_changed();
		break;
	}
	}

	case SR_DF_LOGIC:
	{
		lock_guard<mutex> lock(_data_mutex);
		assert(packet->payload);
		if(!_cur_logic_snapshot)
		{
			// Create a new data snapshot
			_cur_logic_snapshot = shared_ptr<LogicDataSnapshot>(
				new LogicDataSnapshot(
				*(sr_datafeed_logic*)packet->payload));
			_logic_data->push_snapshot(_cur_logic_snapshot);
		}
		else
		{
			// Append to the existing data snapshot
			_cur_logic_snapshot->append_payload(
				*(sr_datafeed_logic*)packet->payload);
		}

		data_updated();
		break;
	}

	case SR_DF_END:
	{
		{
			lock_guard<mutex> lock(_data_mutex);
			_cur_logic_snapshot.reset();
		}
		data_updated();
		break;
	}
	}
}

void SigSession::data_feed_in_proc(const struct sr_dev_inst *sdi,
	struct sr_datafeed_packet *packet)
{
	assert(_session);
	_session->data_feed_in(sdi, packet);
}

} // namespace pv
