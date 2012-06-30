/*
 * This file is part of the sigrok project.
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
#include "logicsignal.h"

#include <QDebug>

#include <assert.h>

using namespace boost;
using namespace std;

// TODO: This should not be necessary
SigSession* SigSession::session = NULL;

SigSession::SigSession()
{
	// TODO: This should not be necessary
	session = this;
}

SigSession::~SigSession()
{
	// TODO: This should not be necessary
	session = NULL;
}

void SigSession::loadFile(const std::string &name)
{
	if (sr_session_load(name.c_str()) == SR_OK) {
		/* sigrok session file */
		sr_session_datafeed_callback_add(dataFeedInProc);
		sr_session_start();
		sr_session_run();
		sr_session_stop();
	}
}

void SigSession::start_capture(struct sr_dev_inst *sdi,
	uint64_t sample_rate)
{
	sr_session_new();
	sr_session_datafeed_callback_add(dataFeedInProc);

	if (sr_session_dev_add(sdi) != SR_OK) {
		qDebug() << "Failed to use device.";
		sr_session_destroy();
		return;
	}

	uint64_t limit_samples = 10000;
	if (sr_dev_config_set(sdi, SR_HWCAP_LIMIT_SAMPLES,
		&limit_samples) != SR_OK) {
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

vector< shared_ptr<Signal> >& SigSession::get_signals()
{
	return _signals;
}

void SigSession::dataFeedIn(const struct sr_dev_inst *sdi,
	struct sr_datafeed_packet *packet)
{
	assert(sdi);
	assert(packet);

	switch (packet->type) {
	case SR_DF_HEADER:
		_signals.clear();
		break;

	case SR_DF_META_LOGIC:
		{
			assert(packet->payload);

			const sr_datafeed_meta_logic &meta_logic =
				*(sr_datafeed_meta_logic*)packet->payload;

			// Create an empty LogiData for coming data snapshots
			_logic_data.reset(new LogicData(meta_logic));
			assert(_logic_data);
			if(!_logic_data)
				break;

			// Add the signals
			for (int i = 0; i < meta_logic.num_probes; i++)
			{
				const sr_probe *const probe =
					(const sr_probe*)g_slist_nth_data(
						sdi->probes, i);
				if(probe->enabled)
				{
					boost::shared_ptr<LogicSignal> signal(
						new LogicSignal(probe->name,
							_logic_data,
							probe->index));
					_signals.push_back(signal);
				}
			}

			break;
		}

	case SR_DF_LOGIC:

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

		break;

	case SR_DF_END:
		_cur_logic_snapshot.reset();
		dataUpdated();
		break;
	}
}

void SigSession::dataFeedInProc(const struct sr_dev_inst *sdi,
	struct sr_datafeed_packet *packet)
{
	assert(session);
	session->dataFeedIn(sdi, packet);
}
