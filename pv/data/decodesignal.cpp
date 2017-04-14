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

#include <limits>

#include "logic.hpp"
#include "logicsegment.hpp"
#include "decodesignal.hpp"
#include "signaldata.hpp"

#include <pv/binding/decoder.hpp>
#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/data/decoderstack.hpp>
#include <pv/session.hpp>

using std::make_shared;
using std::min;
using std::shared_ptr;
using pv::data::decode::Decoder;
using pv::data::decode::Row;

namespace pv {
namespace data {

DecodeSignal::DecodeSignal(shared_ptr<pv::data::DecoderStack> decoder_stack,
	const unordered_set< shared_ptr<data::SignalBase> > &all_signals) :
	SignalBase(nullptr, SignalBase::DecodeChannel),
	decoder_stack_(decoder_stack),
	all_signals_(all_signals)
{
	set_name(QString::fromUtf8(decoder_stack_->stack().front()->decoder()->name));

	update_channel_list();
	auto_assign_signals();

	connect(decoder_stack_.get(), SIGNAL(new_annotations()),
		this, SLOT(on_new_annotations()));
}

DecodeSignal::~DecodeSignal()
{
}

bool DecodeSignal::is_decode_signal() const
{
	return true;
}

shared_ptr<pv::data::DecoderStack> DecodeSignal::decoder_stack() const
{
	return decoder_stack_;
}

const list< shared_ptr<Decoder> >& DecodeSignal::decoder_stack_list() const
{
	return decoder_stack_->stack();
}

void DecodeSignal::stack_decoder(srd_decoder *decoder)
{
	assert(decoder);
	assert(decoder_stack);
	decoder_stack_->push(make_shared<data::decode::Decoder>(decoder));

	// Include the newly created decode channels in the channel list
	update_channel_list();

	auto_assign_signals();
	decoder_stack_->begin_decode();
}

void DecodeSignal::remove_decoder(int index)
{
	decoder_stack_->remove(index);
	update_channel_list();
	decoder_stack_->begin_decode();
}

bool DecodeSignal::toggle_decoder_visibility(int index)
{
	const list< shared_ptr<Decoder> > stack(decoder_stack_->stack());

	auto iter = stack.cbegin();
	for (int i = 0; i < index; i++, iter++)
		assert(iter != stack.end());

	shared_ptr<Decoder> dec = *iter;

	// Toggle decoder visibility
	bool state = false;
	if (dec) {
		state = !dec->shown();
		dec->show(state);
	}

	return state;
}

void DecodeSignal::begin_decode()
{
	decoder_stack_->begin_decode();
}

QString DecodeSignal::error_message() const
{
	return decoder_stack_->error_message();
}

const list<data::DecodeChannel> DecodeSignal::get_channels() const
{
	return channels_;
}

void DecodeSignal::auto_assign_signals()
{
	// Try to auto-select channels that don't have signals assigned yet
	for (data::DecodeChannel &ch : channels_) {
		if (ch.assigned_signal)
			continue;

		for (shared_ptr<data::SignalBase> s : all_signals_)
			if (s->logic_data() && (ch.name.toLower().contains(s->name().toLower())))
				ch.assigned_signal = s.get();
	}
}

void DecodeSignal::assign_signal(const uint16_t channel_id, const SignalBase *signal)
{
	for (data::DecodeChannel &ch : channels_)
		if (ch.id == channel_id)
			ch.assigned_signal = signal;

	channels_updated();

	decoder_stack_->begin_decode();
}

void DecodeSignal::set_initial_pin_state(const uint16_t channel_id, const int init_state)
{
	for (data::DecodeChannel &ch : channels_)
		if (ch.id == channel_id)
			ch.initial_pin_state = init_state;

	channels_updated();

	decoder_stack_->begin_decode();
}

double DecodeSignal::samplerate() const
{
	return decoder_stack_->samplerate();
}

const pv::util::Timestamp& DecodeSignal::start_time() const
{
	return decoder_stack_->start_time();
}

int64_t DecodeSignal::get_working_sample_count() const
{
	// The working sample count is the highest sample number for
	// which all used signals have data available, so go through
	// all channels and use the lowest overall sample count of the
	// current segment

	// TODO Currently, we assume only a single segment exists

	int64_t count = std::numeric_limits<int64_t>::max();
	bool no_signals_assigned = true;

	for (const data::DecodeChannel &ch : channels_)
		if (ch.assigned_signal) {
			no_signals_assigned = false;

			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();
			if (!logic_data || logic_data->logic_segments().empty())
				return 0;

			const shared_ptr<LogicSegment> segment = logic_data->logic_segments().front();
			count = min(count, (int64_t)segment->get_sample_count());
		}

	return (no_signals_assigned ? 0 : count);
}

int64_t DecodeSignal::get_decoded_sample_count() const
{
	return decoder_stack_->samples_decoded();
}

vector<Row> DecodeSignal::visible_rows() const
{
	return decoder_stack_->get_visible_rows();
}

void DecodeSignal::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	const decode::Row &row, uint64_t start_sample,
	uint64_t end_sample) const
{
	return decoder_stack_->get_annotation_subset(dest, row,
		start_sample, end_sample);
}

void DecodeSignal::update_channel_list()
{
	list<data::DecodeChannel> prev_channels = channels_;
	channels_.clear();

	uint16_t id = 0;

	// Copy existing entries, create new as needed
	for (shared_ptr<Decoder> decoder : decoder_stack_->stack()) {
		const srd_decoder* srd_d = decoder->decoder();
		const GSList *l;

		// Mandatory channels
		for (l = srd_d->channels; l; l = l->next) {
			const struct srd_channel *const pdch = (struct srd_channel *)l->data;
			bool ch_added = false;

			// Copy but update ID if this channel was in the list before
			for (data::DecodeChannel ch : prev_channels)
				if (ch.pdch_ == pdch) {
					ch.id = id++;
					channels_.push_back(ch);
					ch_added = true;
					break;
				}

			if (!ch_added) {
				// Create new entry without a mapped signal
				data::DecodeChannel ch = {id++, false, nullptr,
					QString::fromUtf8(pdch->name), QString::fromUtf8(pdch->desc),
					SRD_INITIAL_PIN_SAME_AS_SAMPLE0, decoder, pdch};
				channels_.push_back(ch);
			}
		}

		// Optional channels
		for (l = srd_d->opt_channels; l; l = l->next) {
			const struct srd_channel *const pdch = (struct srd_channel *)l->data;
			bool ch_added = false;

			// Copy but update ID if this channel was in the list before
			for (data::DecodeChannel ch : prev_channels)
				if (ch.pdch_ == pdch) {
					ch.id = id++;
					channels_.push_back(ch);
					ch_added = true;
					break;
				}

			if (!ch_added) {
				// Create new entry without a mapped signal
				data::DecodeChannel ch = {id++, true, nullptr,
					QString::fromUtf8(pdch->name), QString::fromUtf8(pdch->desc),
					SRD_INITIAL_PIN_SAME_AS_SAMPLE0, decoder, pdch};
				channels_.push_back(ch);
			}
		}
	}

	channels_updated();
}

void DecodeSignal::on_new_annotations()
{
	// Forward the signal to the frontend
	new_annotations();
}

} // namespace data
} // namespace pv
