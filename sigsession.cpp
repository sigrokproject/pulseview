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

#include <QDebug>

#include <assert.h>

using namespace boost;

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

void SigSession::dataFeedIn(const struct sr_dev_inst *sdi,
	struct sr_datafeed_packet *packet)
{
	assert(sdi);
	assert(packet);

	switch (packet->type) {
	case SR_DF_HEADER:
		break;

	case SR_DF_META_LOGIC:
		{
			assert(packet->payload);

			_logic_data.reset(new LogicData(
				*(sr_datafeed_meta_logic*)packet->payload));

			assert(_logic_data);
			if(!_logic_data)
				break;

			// Add an empty data snapshot
			shared_ptr<LogicDataSnapshot> snapshot(
				new LogicDataSnapshot());
			_logic_data->push_snapshot(snapshot);
			_cur_logic_snapshot = snapshot;

			break;
		}

	case SR_DF_LOGIC:
		assert(packet->payload);
		assert(_cur_logic_snapshot);
		if(_cur_logic_snapshot)
			_cur_logic_snapshot->append_payload(
				*(sr_datafeed_logic*)packet->payload);
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
