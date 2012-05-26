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

#include <QDebug>

#include <assert.h>

// TODO: This should not be necessary
SigSession* SigSession::session = NULL;

SigSession::SigSession() :
	unitSize(0),
	sigData(NULL)
{
	// TODO: This should not be necessary
	session = this;
}

SigSession::~SigSession()
{
	g_array_free(sigData, TRUE);

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
	case SR_DF_META_LOGIC:
		{
			const sr_datafeed_meta_logic *meta_logic =
				(sr_datafeed_meta_logic*)packet->payload;
			int num_enabled_probes = 0;

			for (int i = 0; i < meta_logic->num_probes; i++) {
				const sr_probe *probe =
					(sr_probe *)g_slist_nth_data(sdi->probes, i);
				if (probe->enabled) {
					probeList[num_enabled_probes++] = probe->index;
				}
			}

			/* How many bytes we need to store num_enabled_probes bits */
			unitSize = (num_enabled_probes + 7) / 8;
			sigData = g_array_new(FALSE, FALSE, unitSize);
		}
		break;

	case SR_DF_LOGIC:
		{
			uint64_t filter_out_len;
			uint8_t *filter_out;

			const struct sr_datafeed_logic *const logic =
				(sr_datafeed_logic*)packet->payload;

			qDebug() << "SR_DF_LOGIC (length =" << logic->length
				<< ", unitsize = " << logic->unitsize << ")";

			if (sr_filter_probes(logic->unitsize, unitSize,
				probeList, (uint8_t*)logic->data, logic->length,
				&filter_out, &filter_out_len) != SR_OK)
				return;

			assert(sigData);
			g_array_append_vals(sigData, filter_out, filter_out_len / unitSize);

			g_free(filter_out);
		}
		break;

	case SR_DF_END:
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
