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

#include <libsigrok/libsigrok.h>
#include <libsigrokdecode/libsigrokdecode.h>

#include "decoder.h"

#include <pv/view/logicsignal.h>

using boost::shared_ptr;
using std::set;
using std::map;
using std::string;

namespace pv {
namespace data {
namespace decode {

Decoder::Decoder(const srd_decoder *const dec) :
	_decoder(dec),
	_shown(true)
{
}

Decoder::~Decoder()
{
	for (map<string, GVariant*>::const_iterator i = _options.begin();
		i != _options.end(); i++)
		g_variant_unref((*i).second);
}

const srd_decoder* Decoder::decoder() const
{
	return _decoder;
}

bool Decoder::shown() const
{
	return _shown;
}

void Decoder::show(bool show)
{
	_shown = show;
}

const map<const srd_probe*, shared_ptr<view::LogicSignal> >&
Decoder::probes() const
{
	return _probes;
}

void Decoder::set_probes(std::map<const srd_probe*,
	boost::shared_ptr<view::LogicSignal> > probes)
{
	_probes = probes;
}

const std::map<std::string, GVariant*>& Decoder::options() const
{
	return _options;
}

void Decoder::set_option(const char *id, GVariant *value)
{
	assert(value);
	g_variant_ref(value);
	_options[id] = value;
}

bool Decoder::have_required_probes() const
{
	for (GSList *p = _decoder->probes; p; p = p->next) {
		const srd_probe *const probe = (const srd_probe*)p->data;
		assert(probe);
		if (_probes.find(probe) == _probes.end())
			return false;
	}

	return true;
}

set< shared_ptr<pv::data::Logic> > Decoder::get_data()
{
	set< shared_ptr<pv::data::Logic> > data;
	for(map<const srd_probe*, shared_ptr<view::LogicSignal> >::
		const_iterator i = _probes.begin();
		i != _probes.end(); i++)
	{
		shared_ptr<view::LogicSignal> signal((*i).second);
		assert(signal);
		data.insert(signal->logic_data());
	}

	return data;
}

srd_decoder_inst* Decoder::create_decoder_inst(srd_session *session, int unit_size) const
{
	GHashTable *const opt_hash = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for (map<string, GVariant*>::const_iterator i = _options.begin();
		i != _options.end(); i++)
	{
		GVariant *const value = (*i).second;
		g_variant_ref(value);
		g_hash_table_replace(opt_hash, (void*)g_strdup(
			(*i).first.c_str()), value);
	}

	srd_decoder_inst *const decoder_inst = srd_inst_new(
		session, _decoder->id, opt_hash);
	g_hash_table_destroy(opt_hash);

	if(!decoder_inst)
		return NULL;

	// Setup the probes
	GHashTable *const probes = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for(map<const srd_probe*, shared_ptr<view::LogicSignal> >::
		const_iterator i = _probes.begin();
		i != _probes.end(); i++)
	{
		shared_ptr<view::LogicSignal> signal((*i).second);
		GVariant *const gvar = g_variant_new_int32(
			signal->probe()->index);
		g_variant_ref_sink(gvar);
		g_hash_table_insert(probes, (*i).first->id, gvar);
	}

	srd_inst_probe_set_all(decoder_inst, probes, unit_size);

	return decoder_inst;
}

} // decode
} // data
} // pv
