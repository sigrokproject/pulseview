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

#include <QDebug>

#include "logic.hpp"
#include "logicsegment.hpp"
#include "decodesignal.hpp"
#include "signaldata.hpp"

#include <pv/binding/decoder.hpp>
#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/globalsettings.hpp>
#include <pv/session.hpp>

using std::lock_guard;
using std::make_pair;
using std::make_shared;
using std::min;
using std::out_of_range;
using std::shared_ptr;
using std::unique_lock;
using pv::data::decode::Annotation;
using pv::data::decode::Decoder;
using pv::data::decode::Row;

namespace pv {
namespace data {

const double DecodeSignal::DecodeMargin = 1.0;
const double DecodeSignal::DecodeThreshold = 0.2;
const int64_t DecodeSignal::DecodeChunkLength = 256 * 1024;


DecodeSignal::DecodeSignal(pv::Session &session) :
	SignalBase(nullptr, SignalBase::DecodeChannel),
	session_(session),
	srd_session_(nullptr),
	logic_mux_data_invalid_(false),
	start_time_(0),
	samplerate_(0),
	samples_decoded_(0),
	frame_complete_(false)
{
	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(on_capture_state_changed(int)));
}

DecodeSignal::~DecodeSignal()
{
	reset_decode();
}

const vector< shared_ptr<Decoder> >& DecodeSignal::decoder_stack() const
{
	return stack_;
}

void DecodeSignal::stack_decoder(const srd_decoder *decoder)
{
	assert(decoder);
	const shared_ptr<Decoder> dec = make_shared<decode::Decoder>(decoder);

	stack_.push_back(dec);

	// Set name if this decoder is the first in the list
	if (stack_.size() == 1)
		set_name(QString::fromUtf8(decoder->name));

	// Include the newly created decode channels in the channel lists
	update_channel_list();

	auto_assign_signals(dec);
	commit_decoder_channels();
	begin_decode();
}

void DecodeSignal::remove_decoder(int index)
{
	assert(index >= 0);
	assert(index < (int)stack_.size());

	// Find the decoder in the stack
	auto iter = stack_.begin();
	for (int i = 0; i < index; i++, iter++)
		assert(iter != stack_.end());

	// Delete the element
	stack_.erase(iter);

	// Update channels and decoded data
	update_channel_list();
	begin_decode();
}

bool DecodeSignal::toggle_decoder_visibility(int index)
{
	auto iter = stack_.cbegin();
	for (int i = 0; i < index; i++, iter++)
		assert(iter != stack_.end());

	shared_ptr<Decoder> dec = *iter;

	// Toggle decoder visibility
	bool state = false;
	if (dec) {
		state = !dec->shown();
		dec->show(state);
	}

	return state;
}

void DecodeSignal::reset_decode()
{
	if (decode_thread_.joinable()) {
		decode_interrupt_ = true;
		decode_input_cond_.notify_one();
		decode_thread_.join();
	}

	if (logic_mux_thread_.joinable()) {
		logic_mux_interrupt_ = true;
		logic_mux_cond_.notify_one();
		logic_mux_thread_.join();
	}

	stop_srd_session();

	frame_complete_ = false;
	samples_decoded_ = 0;
	currently_processed_segment_ = 0;
	error_message_ = QString();

	rows_.clear();
	current_rows_= nullptr;
	class_rows_.clear();

	logic_mux_data_.reset();
	logic_mux_data_invalid_ = true;

	decode_reset();
}

void DecodeSignal::begin_decode()
{
	if (decode_thread_.joinable()) {
		decode_interrupt_ = true;
		decode_input_cond_.notify_one();
		decode_thread_.join();
	}

	if (logic_mux_thread_.joinable()) {
		logic_mux_interrupt_ = true;
		logic_mux_cond_.notify_one();
		logic_mux_thread_.join();
	}

	reset_decode();

	if (stack_.size() == 0) {
		error_message_ = tr("No decoders");
		return;
	}

	assert(channels_.size() > 0);

	if (get_assigned_signal_count() == 0) {
		error_message_ = tr("There are no channels assigned to this decoder");
		return;
	}

	// Make sure that all assigned channels still provide logic data
	// (can happen when a converted signal was assigned but the
	// conversion removed in the meanwhile)
	for (data::DecodeChannel &ch : channels_)
		if (ch.assigned_signal && !(ch.assigned_signal->logic_data() != nullptr))
			ch.assigned_signal = nullptr;

	// Check that all decoders have the required channels
	for (const shared_ptr<decode::Decoder> &dec : stack_)
		if (!dec->have_required_channels()) {
			error_message_ = tr("One or more required channels "
				"have not been specified");
			return;
		}

	// Map out all the annotation classes
	for (const shared_ptr<decode::Decoder> &dec : stack_) {
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);

			const Row row(decc, ann_row);

			for (const GSList *ll = ann_row->ann_classes;
				ll; ll = ll->next)
				class_rows_[make_pair(decc,
					GPOINTER_TO_INT(ll->data))] = row;
		}
	}

	prepare_annotation_segment();

	// Free the logic data and its segment(s) if it needs to be updated
	if (logic_mux_data_invalid_)
		logic_mux_data_.reset();

	if (!logic_mux_data_) {
		const int64_t ch_count = get_assigned_signal_count();
		const int64_t unit_size = (ch_count + 7) / 8;
		logic_mux_data_ = make_shared<Logic>(ch_count);
		logic_mux_segment_ = make_shared<LogicSegment>(*logic_mux_data_, unit_size, samplerate_);
		logic_mux_data_->push_segment(logic_mux_segment_);
	}

	// Make sure the logic output data is complete and up-to-date
	logic_mux_interrupt_ = false;
	logic_mux_thread_ = std::thread(&DecodeSignal::logic_mux_proc, this);

	// Decode the muxed logic data
	decode_interrupt_ = false;
	decode_thread_ = std::thread(&DecodeSignal::decode_proc, this);

	// Receive notifications when new sample data is available
	connect_input_notifiers();
}

QString DecodeSignal::error_message() const
{
	lock_guard<mutex> lock(output_mutex_);
	return error_message_;
}

const vector<data::DecodeChannel> DecodeSignal::get_channels() const
{
	return channels_;
}

void DecodeSignal::auto_assign_signals(const shared_ptr<Decoder> dec)
{
	bool new_assignment = false;

	// Try to auto-select channels that don't have signals assigned yet
	for (data::DecodeChannel &ch : channels_) {
		// If a decoder is given, auto-assign only its channels
		if (dec && (ch.decoder_ != dec))
			continue;

		if (ch.assigned_signal)
			continue;

		for (shared_ptr<data::SignalBase> s : session_.signalbases()) {
			const QString ch_name = ch.name.toLower();
			const QString s_name = s->name().toLower();

			if (s->logic_data() &&
				((ch_name.contains(s_name)) || (s_name.contains(ch_name)))) {
				ch.assigned_signal = s.get();
				new_assignment = true;
			}
		}
	}

	if (new_assignment) {
		logic_mux_data_invalid_ = true;
		commit_decoder_channels();
		channels_updated();
	}
}

void DecodeSignal::assign_signal(const uint16_t channel_id, const SignalBase *signal)
{
	for (data::DecodeChannel &ch : channels_)
		if (ch.id == channel_id) {
			ch.assigned_signal = signal;
			logic_mux_data_invalid_ = true;
		}

	commit_decoder_channels();
	channels_updated();
	begin_decode();
}

int DecodeSignal::get_assigned_signal_count() const
{
	// Count all channels that have a signal assigned to them
	return count_if(channels_.begin(), channels_.end(),
		[](data::DecodeChannel ch) { return ch.assigned_signal; });
}

void DecodeSignal::set_initial_pin_state(const uint16_t channel_id, const int init_state)
{
	for (data::DecodeChannel &ch : channels_)
		if (ch.id == channel_id)
			ch.initial_pin_state = init_state;

	channels_updated();

	begin_decode();
}

double DecodeSignal::samplerate() const
{
	return samplerate_;
}

const pv::util::Timestamp& DecodeSignal::start_time() const
{
	return start_time_;
}

int64_t DecodeSignal::get_working_sample_count(uint32_t segment_id) const
{
	// The working sample count is the highest sample number for
	// which all used signals have data available, so go through
	// all channels and use the lowest overall sample count of the
	// current segment

	int64_t count = std::numeric_limits<int64_t>::max();
	bool no_signals_assigned = true;

	for (const data::DecodeChannel &ch : channels_)
		if (ch.assigned_signal) {
			no_signals_assigned = false;

			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();
			if (!logic_data || logic_data->logic_segments().empty())
				return 0;

			try {
				const shared_ptr<LogicSegment> segment = logic_data->logic_segments().at(segment_id);
				count = min(count, (int64_t)segment->get_sample_count());
			} catch (out_of_range) {
				return 0;
			}
		}

	return (no_signals_assigned ? 0 : count);
}

int64_t DecodeSignal::get_decoded_sample_count(uint32_t segment_id) const
{
	lock_guard<mutex> decode_lock(output_mutex_);

	int64_t result = 0;

	if (segment_id == currently_processed_segment_)
		result = samples_decoded_;
	else
		if (segment_id < currently_processed_segment_)
			// Segment was already decoded fully
			result = get_working_sample_count(segment_id);
		else
			// Segment wasn't decoded at all yet
			result = 0;

	return result;
}

vector<Row> DecodeSignal::visible_rows() const
{
	lock_guard<mutex> lock(output_mutex_);

	vector<Row> rows;

	for (const shared_ptr<decode::Decoder> &dec : stack_) {
		assert(dec);
		if (!dec->shown())
			continue;

		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			rows.emplace_back(decc);

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);
			rows.emplace_back(decc, ann_row);
		}
	}

	return rows;
}

void DecodeSignal::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	const decode::Row &row, uint64_t start_sample,
	uint64_t end_sample) const
{
	lock_guard<mutex> lock(output_mutex_);

	if (!current_rows_)
		return;

	// TODO Instead of current_rows_, use rows_ and the ID of the segment

	const auto iter = current_rows_->find(row);
	if (iter != current_rows_->end())
		(*iter).second.get_annotation_subset(dest,
			start_sample, end_sample);
}

void DecodeSignal::save_settings(QSettings &settings) const
{
	SignalBase::save_settings(settings);

	settings.setValue("decoders", (int)(stack_.size()));

	// Save decoder stack
	int decoder_idx = 0;
	for (shared_ptr<decode::Decoder> decoder : stack_) {
		settings.beginGroup("decoder" + QString::number(decoder_idx++));

		settings.setValue("id", decoder->decoder()->id);

		// Save decoder options
		const map<string, GVariant*>& options = decoder->options();

		settings.setValue("options", (int)options.size());

		// Note: decode::Decoder::options() returns only the options
		// that differ from the default. See binding::Decoder::getter()
		int i = 0;
		for (auto option : options) {
			settings.beginGroup("option" + QString::number(i));
			settings.setValue("name", QString::fromStdString(option.first));
			GlobalSettings::store_gvariant(settings, option.second);
			settings.endGroup();
			i++;
		}

		settings.endGroup();
	}

	// Save channel mapping
	settings.setValue("channels", (int)channels_.size());

	for (unsigned int channel_id = 0; channel_id < channels_.size(); channel_id++) {
		auto channel = find_if(channels_.begin(), channels_.end(),
			[&](data::DecodeChannel ch) { return ch.id == channel_id; });

		if (channel == channels_.end()) {
			qDebug() << "ERROR: Gap in channel index:" << channel_id;
			continue;
		}

		settings.beginGroup("channel" + QString::number(channel_id));

		settings.setValue("name", channel->name);  // Useful for debugging
		settings.setValue("initial_pin_state", channel->initial_pin_state);

		if (channel->assigned_signal)
			settings.setValue("assigned_signal_name", channel->assigned_signal->name());

		settings.endGroup();
	}
}

void DecodeSignal::restore_settings(QSettings &settings)
{
	SignalBase::restore_settings(settings);

	// Restore decoder stack
	GSList *dec_list = g_slist_copy((GSList*)srd_decoder_list());

	int decoders = settings.value("decoders").toInt();

	for (int decoder_idx = 0; decoder_idx < decoders; decoder_idx++) {
		settings.beginGroup("decoder" + QString::number(decoder_idx));

		QString id = settings.value("id").toString();

		for (GSList *entry = dec_list; entry; entry = entry->next) {
			const srd_decoder *dec = (srd_decoder*)entry->data;
			if (!dec)
				continue;

			if (QString::fromUtf8(dec->id) == id) {
				shared_ptr<decode::Decoder> decoder =
					make_shared<decode::Decoder>(dec);

				stack_.push_back(decoder);

				// Restore decoder options that differ from their default
				int options = settings.value("options").toInt();

				for (int i = 0; i < options; i++) {
					settings.beginGroup("option" + QString::number(i));
					QString name = settings.value("name").toString();
					GVariant *value = GlobalSettings::restore_gvariant(settings);
					decoder->set_option(name.toUtf8(), value);
					settings.endGroup();
				}

				// Include the newly created decode channels in the channel lists
				update_channel_list();
				break;
			}
		}

		settings.endGroup();
		channels_updated();
	}

	// Restore channel mapping
	unsigned int channels = settings.value("channels").toInt();

	const unordered_set< shared_ptr<data::SignalBase> > signalbases =
		session_.signalbases();

	for (unsigned int channel_id = 0; channel_id < channels; channel_id++) {
		auto channel = find_if(channels_.begin(), channels_.end(),
			[&](data::DecodeChannel ch) { return ch.id == channel_id; });

		if (channel == channels_.end()) {
			qDebug() << "ERROR: Non-existant channel index:" << channel_id;
			continue;
		}

		settings.beginGroup("channel" + QString::number(channel_id));

		QString assigned_signal_name = settings.value("assigned_signal_name").toString();

		for (shared_ptr<data::SignalBase> signal : signalbases)
			if (signal->name() == assigned_signal_name)
				channel->assigned_signal = signal.get();

		channel->initial_pin_state = settings.value("initial_pin_state").toInt();

		settings.endGroup();
	}

	// Update the internal structures
	update_channel_list();
	commit_decoder_channels();

	begin_decode();
}

void DecodeSignal::update_channel_list()
{
	vector<data::DecodeChannel> prev_channels = channels_;
	channels_.clear();

	uint16_t id = 0;

	// Copy existing entries, create new as needed
	for (shared_ptr<Decoder> decoder : stack_) {
		const srd_decoder* srd_d = decoder->decoder();
		const GSList *l;

		// Mandatory channels
		for (l = srd_d->channels; l; l = l->next) {
			const struct srd_channel *const pdch = (struct srd_channel *)l->data;
			bool ch_added = false;

			// Copy but update ID if this channel was in the list before
			for (data::DecodeChannel &ch : prev_channels)
				if (ch.pdch_ == pdch) {
					ch.id = id++;
					channels_.push_back(ch);
					ch_added = true;
					break;
				}

			if (!ch_added) {
				// Create new entry without a mapped signal
				data::DecodeChannel ch = {id++, 0, false, nullptr,
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
			for (data::DecodeChannel &ch : prev_channels)
				if (ch.pdch_ == pdch) {
					ch.id = id++;
					channels_.push_back(ch);
					ch_added = true;
					break;
				}

			if (!ch_added) {
				// Create new entry without a mapped signal
				data::DecodeChannel ch = {id++, 0, true, nullptr,
					QString::fromUtf8(pdch->name), QString::fromUtf8(pdch->desc),
					SRD_INITIAL_PIN_SAME_AS_SAMPLE0, decoder, pdch};
				channels_.push_back(ch);
			}
		}
	}

	// Invalidate the logic output data if the channel assignment changed
	if (prev_channels.size() != channels_.size()) {
		// The number of channels changed, there's definitely a difference
		logic_mux_data_invalid_ = true;
	} else {
		// Same number but assignment may still differ, so compare all channels
		for (size_t i = 0; i < channels_.size(); i++) {
			const data::DecodeChannel &p_ch = prev_channels[i];
			const data::DecodeChannel &ch = channels_[i];

			if ((p_ch.pdch_ != ch.pdch_) ||
				(p_ch.assigned_signal != ch.assigned_signal)) {
				logic_mux_data_invalid_ = true;
				break;
			}
		}

	}

	channels_updated();
}

void DecodeSignal::commit_decoder_channels()
{
	// Submit channel list to every decoder, containing only the relevant channels
	for (shared_ptr<decode::Decoder> dec : stack_) {
		vector<data::DecodeChannel*> channel_list;

		for (data::DecodeChannel &ch : channels_)
			if (ch.decoder_ == dec)
				channel_list.push_back(&ch);

		dec->set_channels(channel_list);
	}

	// Channel bit IDs must be in sync with the channel's apperance in channels_
	int id = 0;
	for (data::DecodeChannel &ch : channels_)
		if (ch.assigned_signal)
			ch.bit_id = id++;
}

void DecodeSignal::mux_logic_samples(const int64_t start, const int64_t end)
{
	// Enforce end to be greater than start
	if (end <= start)
		return;

	// Fetch all segments and their data
	// TODO Currently, we assume only a single segment exists
	vector<shared_ptr<LogicSegment> > segments;
	vector<const uint8_t*> signal_data;
	vector<uint8_t> signal_in_bytepos;
	vector<uint8_t> signal_in_bitpos;

	for (data::DecodeChannel &ch : channels_)
		if (ch.assigned_signal) {
			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();
			const shared_ptr<LogicSegment> segment = logic_data->logic_segments().front();
			segments.push_back(segment);

			uint8_t* data = new uint8_t[(end - start) * segment->unit_size()];
			segment->get_samples(start, end, data);
			signal_data.push_back(data);

			const int bitpos = ch.assigned_signal->logic_bit_index();
			signal_in_bytepos.push_back(bitpos / 8);
			signal_in_bitpos.push_back(bitpos % 8);
		}

	// Perform the muxing of signal data into the output data
	uint8_t* output = new uint8_t[(end - start) * logic_mux_segment_->unit_size()];
	unsigned int signal_count = signal_data.size();

	for (int64_t sample_cnt = 0; sample_cnt < (end - start); sample_cnt++) {
		int bitpos = 0;
		uint8_t bytepos = 0;

		const int out_sample_pos = sample_cnt * logic_mux_segment_->unit_size();
		for (unsigned int i = 0; i < logic_mux_segment_->unit_size(); i++)
			output[out_sample_pos + i] = 0;

		for (unsigned int i = 0; i < signal_count; i++) {
			const int in_sample_pos = sample_cnt * segments[i]->unit_size();
			const uint8_t in_sample = 1 &
				((signal_data[i][in_sample_pos + signal_in_bytepos[i]]) >> (signal_in_bitpos[i]));

			const uint8_t out_sample = output[out_sample_pos + bytepos];

			output[out_sample_pos + bytepos] = out_sample | (in_sample << bitpos);

			bitpos++;
			if (bitpos > 7) {
				bitpos = 0;
				bytepos++;
			}
		}
	}

	logic_mux_segment_->append_payload(output, (end - start) * logic_mux_segment_->unit_size());
	delete[] output;

	for (const uint8_t* data : signal_data)
		delete[] data;
}

void DecodeSignal::logic_mux_proc()
{
	do {
		const uint64_t input_sample_count = get_working_sample_count(currently_processed_segment_);
		const uint64_t output_sample_count = logic_mux_segment_->get_sample_count();

		const uint64_t samples_to_process =
			(input_sample_count > output_sample_count) ?
			(input_sample_count - output_sample_count) : 0;

		// Process the samples if necessary...
		if (samples_to_process > 0) {
			const uint64_t unit_size = logic_mux_segment_->unit_size();
			const uint64_t chunk_sample_count = DecodeChunkLength / unit_size;

			uint64_t processed_samples = 0;
			do {
				const uint64_t start_sample = output_sample_count + processed_samples;
				const uint64_t sample_count =
					min(samples_to_process - processed_samples,	chunk_sample_count);

				mux_logic_samples(start_sample, start_sample + sample_count);
				processed_samples += sample_count;

				// ...and process the newly muxed logic data
				decode_input_cond_.notify_one();
			} while (processed_samples < samples_to_process);
		}

		if (samples_to_process == 0) {
			// Wait for more input
			unique_lock<mutex> logic_mux_lock(logic_mux_mutex_);
			logic_mux_cond_.wait(logic_mux_lock);
		}
	} while (!logic_mux_interrupt_);
}

void DecodeSignal::query_input_metadata()
{
	// Update the samplerate and start time because we cannot start
	// the libsrd session without the current samplerate

	// TODO Currently we assume all channels have the same sample rate
	// and start time
	bool samplerate_valid = false;
	data::DecodeChannel *any_channel;
	shared_ptr<Logic> logic_data;

	do {
		any_channel = &(*find_if(channels_.begin(), channels_.end(),
			[](data::DecodeChannel ch) { return ch.assigned_signal; }));

		logic_data = any_channel->assigned_signal->logic_data();

		if (!logic_data) {
			// Wait until input data is available or an interrupt was requested
			unique_lock<mutex> input_wait_lock(input_mutex_);
			decode_input_cond_.wait(input_wait_lock);
		}
	} while (!logic_data && !decode_interrupt_);

	if (decode_interrupt_)
		return;

	do {
		if (!logic_data->logic_segments().empty()) {
			shared_ptr<LogicSegment> first_segment =
				any_channel->assigned_signal->logic_data()->logic_segments().front();
			start_time_ = first_segment->start_time();
			samplerate_ = first_segment->samplerate();
			if (samplerate_ > 0)
				samplerate_valid = true;
		}

		if (!samplerate_valid) {
			// Wait until input data is available or an interrupt was requested
			unique_lock<mutex> input_wait_lock(input_mutex_);
			decode_input_cond_.wait(input_wait_lock);
		}
	} while (!samplerate_valid && !decode_interrupt_);
}

void DecodeSignal::decode_data(
	const int64_t abs_start_samplenum, const int64_t sample_count)
{
	const int64_t unit_size = logic_mux_segment_->unit_size();
	const int64_t chunk_sample_count = DecodeChunkLength / unit_size;

	for (int64_t i = abs_start_samplenum;
		!decode_interrupt_ && (i < (abs_start_samplenum + sample_count));
		i += chunk_sample_count) {

		const int64_t chunk_end = min(i + chunk_sample_count,
			abs_start_samplenum + sample_count);

		int64_t data_size = (chunk_end - i) * unit_size;
		uint8_t* chunk = new uint8_t[data_size];
		logic_mux_segment_->get_samples(i, chunk_end, chunk);

		if (srd_session_send(srd_session_, i, chunk_end, chunk,
				data_size, unit_size) != SRD_OK) {
			error_message_ = tr("Decoder reported an error");
			delete[] chunk;
			break;
		}

		delete[] chunk;

		{
			lock_guard<mutex> lock(output_mutex_);
			samples_decoded_ = chunk_end;
		}

		// Notify the frontend that we processed some data and
		// possibly have new annotations as well
		new_annotations();
	}
}

void DecodeSignal::decode_proc()
{
	query_input_metadata();

	if (decode_interrupt_)
		return;

	start_srd_session();

	uint64_t sample_count;
	uint64_t abs_start_samplenum = 0;
	do {
		// Keep processing new samples until we exhaust the input data
		do {
			lock_guard<mutex> input_lock(input_mutex_);
			sample_count = logic_mux_segment_->get_sample_count() - abs_start_samplenum;

			if (sample_count > 0) {
				decode_data(abs_start_samplenum, sample_count);
				abs_start_samplenum += sample_count;
			}
		} while (error_message_.isEmpty() && (sample_count > 0) && !decode_interrupt_);

		if (error_message_.isEmpty() && !decode_interrupt_) {
			if (sample_count == 0)
				decode_finished();

			// Wait for new input data or an interrupt was requested
			unique_lock<mutex> input_wait_lock(input_mutex_);
			decode_input_cond_.wait(input_wait_lock);
		}
	} while (error_message_.isEmpty() && !decode_interrupt_);
}

void DecodeSignal::start_srd_session()
{
	if (srd_session_)
		stop_srd_session();

	// Create the session
	srd_session_new(&srd_session_);
	assert(srd_session_);

	// Create the decoders
	srd_decoder_inst *prev_di = nullptr;
	for (const shared_ptr<decode::Decoder> &dec : stack_) {
		srd_decoder_inst *const di = dec->create_decoder_inst(srd_session_);

		if (!di) {
			error_message_ = tr("Failed to create decoder instance");
			srd_session_destroy(srd_session_);
			return;
		}

		if (prev_di)
			srd_inst_stack(srd_session_, prev_di, di);

		prev_di = di;
	}

	// Start the session
	srd_session_metadata_set(srd_session_, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64(samplerate_));

	srd_pd_output_callback_add(srd_session_, SRD_OUTPUT_ANN,
		DecodeSignal::annotation_callback, this);

	srd_session_start(srd_session_);
}

void DecodeSignal::stop_srd_session()
{
	if (srd_session_) {
		// Destroy the session
		srd_session_destroy(srd_session_);
		srd_session_ = nullptr;
	}
}

void DecodeSignal::connect_input_notifiers()
{
	// Disconnect the notification slot from the previous set of signals
	disconnect(this, SLOT(on_data_cleared()));
	disconnect(this, SLOT(on_data_received()));

	// Connect the currently used signals to our slot
	for (data::DecodeChannel &ch : channels_) {
		if (!ch.assigned_signal)
			continue;

		const data::SignalBase *signal = ch.assigned_signal;
		connect(signal, SIGNAL(samples_cleared()),
			this, SLOT(on_data_cleared()));
		connect(signal, SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
			this, SLOT(on_data_received()));
	}
}

void DecodeSignal::prepare_annotation_segment()
{
	// TODO Won't work for multiple segments
	rows_.emplace_back(map<const decode::Row, decode::RowData>());
	current_rows_ = &(rows_.back());

	// Add annotation classes
	for (const shared_ptr<decode::Decoder> &dec : stack_) {
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			(*current_rows_)[Row(decc)] = decode::RowData();

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);

			const Row row(decc, ann_row);

			// Add a new empty row data object
			(*current_rows_)[row] = decode::RowData();
		}
	}
}

void DecodeSignal::annotation_callback(srd_proto_data *pdata, void *decode_signal)
{
	assert(pdata);
	assert(decode_signal);

	DecodeSignal *const ds = (DecodeSignal*)decode_signal;
	assert(ds);

	lock_guard<mutex> lock(ds->output_mutex_);

	// Find the row
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);
	assert(ds->current_rows_);

	const srd_proto_data_annotation *const pda =
		(const srd_proto_data_annotation*)pdata->data;
	assert(pda);

	auto row_iter = ds->current_rows_->end();

	// Try looking up the sub-row of this class
	const auto format = pda->ann_class;
	const auto r = ds->class_rows_.find(make_pair(decc, format));
	if (r != ds->class_rows_.end())
		row_iter = ds->current_rows_->find((*r).second);
	else {
		// Failing that, use the decoder as a key
		row_iter = ds->current_rows_->find(Row(decc));
	}

	if (row_iter == ds->current_rows_->end()) {
		qDebug() << "Unexpected annotation: decoder = " << decc <<
			", format = " << format;
		assert(false);
		return;
	}

	// Add the annotation
	(*row_iter).second.emplace_annotation(pdata);
}

void DecodeSignal::on_capture_state_changed(int state)
{
	// If a new acquisition was started, we need to start decoding from scratch
	if (state == Session::Running)
		begin_decode();
}

void DecodeSignal::on_data_cleared()
{
	reset_decode();
}

void DecodeSignal::on_data_received()
{
	if (!logic_mux_thread_.joinable())
		begin_decode();
	else
		logic_mux_cond_.notify_one();
}

} // namespace data
} // namespace pv
