/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
#define PULSEVIEW_PV_DATA_DECODESIGNAL_HPP

#include <unordered_set>
#include <vector>

#include <QString>

#include <libsigrokdecode/libsigrokdecode.h>

#include <pv/data/signalbase.hpp>

using std::list;
using std::unordered_set;
using std::vector;
using std::shared_ptr;

namespace pv {
namespace data {

namespace decode {
class Annotation;
class Decoder;
class Row;
}

class DecoderStack;
class Logic;
class SignalBase;
class SignalData;

struct DecodeChannel
{
	uint16_t id;  // Also tells which bit within a sample represents this channel
	const bool is_optional;
	const pv::data::SignalBase *assigned_signal;
	const QString name, desc;
	int initial_pin_state;
	const shared_ptr<pv::data::decode::Decoder> decoder_;
	const srd_channel *pdch_;
};

class DecodeSignal : public SignalBase
{
	Q_OBJECT

public:
	DecodeSignal(shared_ptr<pv::data::DecoderStack> decoder_stack,
		const unordered_set< shared_ptr<data::SignalBase> > &all_signals);
	virtual ~DecodeSignal();

	bool is_decode_signal() const;
	shared_ptr<data::DecoderStack> decoder_stack() const;
	const list< shared_ptr<data::decode::Decoder> >& decoder_stack_list() const;

	void stack_decoder(srd_decoder *decoder);
	void remove_decoder(int index);
	bool toggle_decoder_visibility(int index);

	QString error_message() const;

	const list<data::DecodeChannel> get_channels() const;
	void auto_assign_signals();
	void assign_signal(const uint16_t channel_id, const SignalBase *signal);

	void set_initial_pin_state(const uint16_t channel_id, const int init_state);

	vector<decode::Row> visible_rows() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		vector<pv::data::decode::Annotation> &dest,
		const decode::Row &row, uint64_t start_sample,
		uint64_t end_sample) const;

private:
	void update_channel_list();

Q_SIGNALS:
	void new_annotations();
	void channels_updated();

private Q_SLOTS:
	void on_new_annotations();

private:
	shared_ptr<pv::data::DecoderStack> decoder_stack_;
	const unordered_set< shared_ptr<data::SignalBase> > &all_signals_;
	list<data::DecodeChannel> channels_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
