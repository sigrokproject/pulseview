/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <libsigrokdecode/libsigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include <boost/test/unit_test.hpp>

#include <libsigrok/libsigrok.h>

#include "../../pv/devicemanager.h"
#include "../../pv/sigsession.h"
#include "../../pv/view/signal.h"

using namespace boost;
using namespace std;

BOOST_AUTO_TEST_SUITE(DecoderTest)

BOOST_AUTO_TEST_CASE(TwoDecoder)
{
	sr_context *ctx = NULL;

	BOOST_REQUIRE(sr_init(&ctx) == SR_OK);
	BOOST_REQUIRE(ctx);

	BOOST_REQUIRE(srd_init(NULL) == SRD_OK);

	srd_decoder_load_all();

	{
		pv::DeviceManager dm(ctx);
		pv::SigSession ss(dm);

		const GSList *l = srd_decoder_list();
		BOOST_REQUIRE(l);
		srd_decoder *const dec = (struct srd_decoder*)l->data;
		BOOST_REQUIRE(dec);

		map<const srd_probe*, shared_ptr<pv::view::Signal> > probes;
		BOOST_CHECK (ss.add_decoder(dec, probes,
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify)g_variant_unref)));

		BOOST_CHECK (ss.add_decoder(dec, probes,
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify)g_variant_unref)));
	}


	srd_exit();
	sr_exit(ctx);
}

BOOST_AUTO_TEST_SUITE_END()
