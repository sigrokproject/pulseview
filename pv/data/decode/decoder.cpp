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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>

#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrokdecode/libsigrokdecode.h>

#include "decoder.hpp"

#include <pv/data/signalbase.hpp>
#include <pv/data/decodesignal.hpp>

using pv::data::DecodeChannel;
using std::set;
using std::map;
using std::shared_ptr;
using std::string;

namespace pv {
namespace data {
namespace decode {

Decoder::Decoder(const srd_decoder *const dec) :
	decoder_(dec),
	shown_(true)
{
}

Decoder::~Decoder()
{
	for (auto& option : options_)
		g_variant_unref(option.second);
}

const srd_decoder* Decoder::decoder() const
{
	return decoder_;
}

bool Decoder::shown() const
{
	return shown_;
}

void Decoder::show(bool show)
{
	shown_ = show;
}

const vector<DecodeChannel*>& Decoder::channels() const
{
	return channels_;
}

void Decoder::set_channels(vector<DecodeChannel*> channels)
{
	channels_ = channels;
}

const map<string, GVariant*>& Decoder::options() const
{
	return options_;
}

void Decoder::set_option(const char *id, GVariant *value)
{
	assert(value);
	g_variant_ref(value);
	options_[id] = value;
}

bool Decoder::have_required_channels() const
{
	for (DecodeChannel *ch : channels_)
		if (!ch->assigned_signal && !ch->is_optional)
			return false;

	return true;
}

srd_decoder_inst* Decoder::create_decoder_inst(srd_session *session) const
{
	GHashTable *const opt_hash = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for (const auto& option : options_) {
		GVariant *const value = option.second;
		g_variant_ref(value);
		g_hash_table_replace(opt_hash, (void*)g_strdup(
			option.first.c_str()), value);
	}

	srd_decoder_inst *const decoder_inst = srd_inst_new(
		session, decoder_->id, opt_hash);
	g_hash_table_destroy(opt_hash);

	if (!decoder_inst)
		return nullptr;

	// Setup the channels
	GArray *const init_pin_states = g_array_sized_new(FALSE, TRUE,
		sizeof(uint8_t), channels_.size());

	g_array_set_size(init_pin_states, channels_.size());

	GHashTable *const channels = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for (DecodeChannel *ch : channels_) {
		init_pin_states->data[ch->id] = ch->initial_pin_state;

		GVariant *const gvar = g_variant_new_int32(ch->id);  // id = bit position
		g_variant_ref_sink(gvar);
		// key is channel name, value is bit position in each sample
		g_hash_table_insert(channels, ch->pdch_->id, gvar);
	}

	srd_inst_channel_set_all(decoder_inst, channels);

	srd_inst_initial_pins_set_all(decoder_inst, init_pin_states);
	g_array_free(init_pin_states, TRUE);

	return decoder_inst;
}

}  // namespace decode
}  // namespace data
}  // namespace pv
