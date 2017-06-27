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
#include <pv/session.hpp>

using std::lock_guard;
using std::make_pair;
using std::make_shared;
using std::min;
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

mutex DecodeSignal::global_srd_mutex_;


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

	set_name(tr("Empty decoder signal"));
}

DecodeSignal::~DecodeSignal()
{
	reset_decode();
}

const vector< shared_ptr<Decoder> >& DecodeSignal::decoder_stack() const
{
	return stack_;
}

void DecodeSignal::stack_decoder(srd_decoder *decoder)
{
	assert(decoder);
	stack_.push_back(make_shared<decode::Decoder>(decoder));

	// Set name if this decoder is the first in the list
	if (stack_.size() == 1)
		set_name(QString::fromUtf8(decoder->name));

	// Include the newly created decode channels in the channel lists
	update_channel_list();

	auto_assign_signals();
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
	error_message_ = QString();

	rows_.clear();
	class_rows_.clear();

	logic_mux_data_.reset();
	logic_mux_data_invalid_ = true;
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

	// Add annotation classes
	for (const shared_ptr<decode::Decoder> &dec : stack_) {
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			rows_[Row(decc)] = decode::RowData();

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);

			const Row row(decc, ann_row);

			// Add a new empty row data object
			rows_[row] = decode::RowData();

			// Map out all the classes
			for (const GSList *ll = ann_row->ann_classes;
				ll; ll = ll->next)
				class_rows_[make_pair(decc,
					GPOINTER_TO_INT(ll->data))] = row;
		}
	}

	// Free the logic data and its segment(s) if it needs to be updated
	if (logic_mux_data_invalid_)
		logic_mux_data_.reset();

	if (!logic_mux_data_) {
		const int64_t ch_count = get_assigned_signal_count();
		const int64_t unit_size = (ch_count + 7) / 8;
		logic_mux_data_ = make_shared<Logic>(ch_count);
		segment_ = make_shared<LogicSegment>(*logic_mux_data_, unit_size, samplerate_);
		logic_mux_data_->push_segment(segment_);
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

void DecodeSignal::auto_assign_signals()
{
	bool new_assignment = false;

	// Try to auto-select channels that don't have signals assigned yet
	for (data::DecodeChannel &ch : channels_) {
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
	lock_guard<mutex> decode_lock(output_mutex_);
	return samples_decoded_;
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

	const auto iter = rows_.find(row);
	if (iter != rows_.end())
		(*iter).second.get_annotation_subset(dest,
			start_sample, end_sample);
}

void DecodeSignal::save_settings(QSettings &settings) const
{
	SignalBase::save_settings(settings);

	// TODO Save decoder stack, channel mapping and decoder options
}

void DecodeSignal::restore_settings(QSettings &settings)
{
	SignalBase::restore_settings(settings);

	// TODO Restore decoder stack, channel mapping and decoder options
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
			for (data::DecodeChannel &ch : prev_channels)
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
			signal_data.push_back(segment->get_samples(start, end));

			const int bitpos = ch.assigned_signal->logic_bit_index();
			signal_in_bytepos.push_back(bitpos / 8);
			signal_in_bitpos.push_back(bitpos % 8);
		}

	// Perform the muxing of signal data into the output data
	uint8_t* output = new uint8_t[(end - start) * segment_->unit_size()];
	unsigned int signal_count = signal_data.size();

	for (int64_t sample_cnt = 0; sample_cnt < (end - start); sample_cnt++) {
		int bitpos = 0;
		uint8_t bytepos = 0;

		const int out_sample_pos = sample_cnt * segment_->unit_size();
		for (unsigned int i = 0; i < segment_->unit_size(); i++)
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

	segment_->append_payload(output, (end - start) * segment_->unit_size());
	delete[] output;

	for (const uint8_t* data : signal_data)
		delete[] data;
}

void DecodeSignal::logic_mux_proc()
{
	do {
		const uint64_t input_sample_count = get_working_sample_count();
		const uint64_t output_sample_count = segment_->get_sample_count();

		const uint64_t samples_to_process =
			(input_sample_count > output_sample_count) ?
			(input_sample_count - output_sample_count) : 0;

		// Process the samples if necessary...
		if (samples_to_process > 0) {
			const uint64_t unit_size = segment_->unit_size();
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

	// No more input data and session is stopped, let the decode thread
	// process any pending data, terminate and release the global SRD mutex
	// in order to let other decoders run
	decode_input_cond_.notify_one();
}

void DecodeSignal::query_input_metadata()
{
	// Update the samplerate and start time because we cannot start
	// the libsrd session without the current samplerate

	// TODO Currently we assume all channels have the same sample rate
	// and start time
	bool samplerate_valid = false;

	auto any_channel = find_if(channels_.begin(), channels_.end(),
		[](data::DecodeChannel ch) { return ch.assigned_signal; });

	shared_ptr<Logic> logic_data =
		any_channel->assigned_signal->logic_data();

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
	const int64_t unit_size = segment_->unit_size();
	const int64_t chunk_sample_count = DecodeChunkLength / unit_size;

	for (int64_t i = abs_start_samplenum;
		!decode_interrupt_ && (i < (abs_start_samplenum + sample_count));
		i += chunk_sample_count) {

		const int64_t chunk_end = min(i + chunk_sample_count,
			abs_start_samplenum + sample_count);

		const uint8_t* chunk = segment_->get_samples(i, chunk_end);

		if (srd_session_send(srd_session_, i, chunk_end, chunk,
				(chunk_end - i) * unit_size, unit_size) != SRD_OK) {
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
			// Prevent any other decode threads from accessing libsigrokdecode
			lock_guard<mutex> srd_lock(global_srd_mutex_);

			{
				lock_guard<mutex> input_lock(input_mutex_);
				sample_count = segment_->get_sample_count() - abs_start_samplenum;
			}

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

void DecodeSignal::annotation_callback(srd_proto_data *pdata, void *decode_signal)
{
	assert(pdata);
	assert(decoder);

	DecodeSignal *const ds = (DecodeSignal*)decode_signal;
	assert(ds);

	lock_guard<mutex> lock(ds->output_mutex_);

	const decode::Annotation a(pdata);

	// Find the row
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

	auto row_iter = ds->rows_.end();

	// Try looking up the sub-row of this class
	const auto r = ds->class_rows_.find(make_pair(decc, a.format()));
	if (r != ds->class_rows_.end())
		row_iter = ds->rows_.find((*r).second);
	else {
		// Failing that, use the decoder as a key
		row_iter = ds->rows_.find(Row(decc));
	}

	assert(row_iter != ds->rows_.end());
	if (row_iter == ds->rows_.end()) {
		qDebug() << "Unexpected annotation: decoder = " << decc <<
			", format = " << a.format();
		assert(false);
		return;
	}

	// Add the annotation
	(*row_iter).second.push_annotation(a);
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
