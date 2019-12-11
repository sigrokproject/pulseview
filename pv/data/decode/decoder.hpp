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

#ifndef PULSEVIEW_PV_DATA_DECODE_DECODER_HPP
#define PULSEVIEW_PV_DATA_DECODE_DECODER_HPP

#include <map>
#include <memory>
#include <set>
#include <vector>

#include <glib.h>

#include <pv/data/signalbase.hpp>

using std::map;
using std::string;
using std::vector;

struct srd_decoder;
struct srd_decoder_inst;
struct srd_channel;
struct srd_session;

namespace pv {

namespace data {

class Logic;
class SignalBase;

namespace decode {

class Decoder;

struct DecodeChannel
{
	uint16_t id;     ///< Global numerical ID for the decode channels in the stack
	uint16_t bit_id; ///< Tells which bit within a sample represents this channel
	const bool is_optional;
	const pv::data::SignalBase *assigned_signal;
	const QString name, desc;
	int initial_pin_state;
	const shared_ptr<Decoder> decoder_;
	const srd_channel *pdch_;
};

struct DecodeBinaryClassInfo
{
	uint8_t bin_class_id;
	char* name;
	char* description;
};


class Decoder
{
public:
	Decoder(const srd_decoder *const dec);

	virtual ~Decoder();

	const srd_decoder* decoder() const;

	const char* name() const;

	bool shown() const;
	void show(bool show = true);

	const vector<DecodeChannel*>& channels() const;
	void set_channels(vector<DecodeChannel*> channels);

	const map<string, GVariant*>& options() const;

	void set_option(const char *id, GVariant *value);

	void apply_all_options();

	bool have_required_channels() const;

	srd_decoder_inst* create_decoder_inst(srd_session *session);
	void invalidate_decoder_inst();

	uint8_t get_binary_class_count() const;
	const DecodeBinaryClassInfo* get_binary_class(uint8_t id) const;

private:
	const srd_decoder *const decoder_;

	bool shown_;

	vector<DecodeChannel*> channels_;
	vector<DecodeBinaryClassInfo> bin_classes_;
	map<string, GVariant*> options_;
	srd_decoder_inst *decoder_inst_;
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODE_DECODER_HPP
