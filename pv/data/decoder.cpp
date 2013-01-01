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

#include <libsigrokdecode/libsigrokdecode.h>

#include "decoder.h"

#include <pv/view/signal.h>

using namespace boost;
using namespace std;

namespace pv {
namespace data {

Decoder::Decoder(const srd_decoder *const dec,
	std::map<const srd_probe*,
		boost::shared_ptr<pv::view::Signal> > probes) :
	_decoder(dec),
	_probes(probes),
	_decoder_inst(NULL)
{
	init_decoder();
}

const srd_decoder* Decoder::get_decoder() const
{
	return _decoder;
}

void Decoder::init_decoder()
{
	_decoder_inst = srd_inst_new(_decoder->id, NULL);
	assert(_decoder_inst);

	GHashTable *probes = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for(map<const srd_probe*, shared_ptr<view::Signal> >::
		const_iterator i = _probes.begin();
		i != _probes.end(); i++)
	{
		shared_ptr<view::Signal> signal((*i).second);
		GVariant *const gvar = g_variant_new_int32(
			signal->probe()->index);
		g_variant_ref_sink(gvar);
		g_hash_table_insert(probes, (*i).first->id, gvar);
	}

	srd_inst_probe_set_all(_decoder_inst, probes);
}

void Decoder::clear_snapshots()
{
}

} // namespace data
} // namespace pv
