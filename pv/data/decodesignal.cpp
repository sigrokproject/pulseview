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

#include <cstring>
#include <forward_list>
#include <limits>

#include <QDebug>

#include "logic.hpp"
#include "logicsegment.hpp"
#include "decodesignal.hpp"
#include "signaldata.hpp"

#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/globalsettings.hpp>
#include <pv/session.hpp>

using std::forward_list;
using std::lock_guard;
using std::make_pair;
using std::make_shared;
using std::min;
using std::out_of_range;
using std::shared_ptr;
using std::unique_lock;
using pv::data::decode::Annotation;
using pv::data::decode::DecodeChannel;
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
	stack_config_changed_(true),
	current_segment_id_(0)
{
	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(on_capture_state_changed(int)));
}

DecodeSignal::~DecodeSignal()
{
	reset_decode(true);
}

const vector< shared_ptr<Decoder> >& DecodeSignal::decoder_stack() const
{
	return stack_;
}

void DecodeSignal::stack_decoder(const srd_decoder *decoder, bool restart_decode)
{
	assert(decoder);

	// Set name if this decoder is the first in the list or the name is unchanged
	const srd_decoder* prev_dec =
		stack_.empty() ? nullptr : stack_.back()->decoder();
	const QString prev_dec_name =
		prev_dec ? QString::fromUtf8(prev_dec->name) : QString();

	if ((stack_.empty()) || ((stack_.size() > 0) && (name() == prev_dec_name)))
		set_name(QString::fromUtf8(decoder->name));

	const shared_ptr<Decoder> dec = make_shared<decode::Decoder>(decoder);
	stack_.push_back(dec);

	// Include the newly created decode channels in the channel lists
	update_channel_list();

	stack_config_changed_ = true;
	auto_assign_signals(dec);
	commit_decoder_channels();

	decoder_stacked((void*)dec.get());

	if (restart_decode)
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

	decoder_removed(iter->get());

	// Delete the element
	stack_.erase(iter);

	// Update channels and decoded data
	stack_config_changed_ = true;
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

void DecodeSignal::reset_decode(bool shutting_down)
{
	if (stack_config_changed_ || shutting_down)
		stop_srd_session();
	else
		terminate_srd_session();

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

	resume_decode();  // Make sure the decode thread isn't blocked by pausing

	class_rows_.clear();
	current_segment_id_ = 0;
	segments_.clear();

	logic_mux_data_.reset();
	logic_mux_data_invalid_ = true;

	if (!error_message_.isEmpty()) {
		error_message_ = QString();
		// TODO Emulate noquote()
		qDebug().nospace() << name() << ": Error cleared";
	}

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
		set_error_message(tr("No decoders"));
		return;
	}

	assert(channels_.size() > 0);

	if (get_assigned_signal_count() == 0) {
		set_error_message(tr("There are no channels assigned to this decoder"));
		return;
	}

	// Make sure that all assigned channels still provide logic data
	// (can happen when a converted signal was assigned but the
	// conversion removed in the meanwhile)
	for (decode::DecodeChannel& ch : channels_)
		if (ch.assigned_signal && !(ch.assigned_signal->logic_data() != nullptr))
			ch.assigned_signal = nullptr;

	// Check that all decoders have the required channels
	for (const shared_ptr<decode::Decoder>& dec : stack_)
		if (!dec->have_required_channels()) {
			set_error_message(tr("One or more required channels "
				"have not been specified"));
			return;
		}

	// Map out all the annotation classes
	int row_index = 0;
	for (const shared_ptr<decode::Decoder>& dec : stack_) {
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);

			const Row row(row_index++, dec.get(), ann_row);

			for (const GSList *ll = ann_row->ann_classes;
				ll; ll = ll->next)
				class_rows_[make_pair(decc, GPOINTER_TO_INT(ll->data))] = row;
		}
	}

	// Free the logic data and its segment(s) if it needs to be updated
	if (logic_mux_data_invalid_)
		logic_mux_data_.reset();

	if (!logic_mux_data_) {
		const uint32_t ch_count = get_assigned_signal_count();
		logic_mux_unit_size_ = (ch_count + 7) / 8;
		logic_mux_data_ = make_shared<Logic>(ch_count);
	}

	// Receive notifications when new sample data is available
	connect_input_notifiers();

	if (get_input_segment_count() == 0) {
		set_error_message(tr("No input data"));
		return;
	}

	// Make sure the logic output data is complete and up-to-date
	logic_mux_interrupt_ = false;
	logic_mux_thread_ = std::thread(&DecodeSignal::logic_mux_proc, this);

	// Decode the muxed logic data
	decode_interrupt_ = false;
	decode_thread_ = std::thread(&DecodeSignal::decode_proc, this);
}

void DecodeSignal::pause_decode()
{
	decode_paused_ = true;
}

void DecodeSignal::resume_decode()
{
	// Manual unlocking is done before notifying, to avoid waking up the
	// waiting thread only to block again (see notify_one for details)
	decode_pause_mutex_.unlock();
	decode_pause_cond_.notify_one();
	decode_paused_ = false;
}

bool DecodeSignal::is_paused() const
{
	return decode_paused_;
}

QString DecodeSignal::error_message() const
{
	lock_guard<mutex> lock(output_mutex_);
	return error_message_;
}

const vector<decode::DecodeChannel> DecodeSignal::get_channels() const
{
	return channels_;
}

void DecodeSignal::auto_assign_signals(const shared_ptr<Decoder> dec)
{
	bool new_assignment = false;

	// Try to auto-select channels that don't have signals assigned yet
	for (decode::DecodeChannel& ch : channels_) {
		// If a decoder is given, auto-assign only its channels
		if (dec && (ch.decoder_ != dec))
			continue;

		if (ch.assigned_signal)
			continue;

		QString ch_name = ch.name.toLower();
		ch_name = ch_name.replace(QRegExp("[-_.]"), " ");

		shared_ptr<data::SignalBase> match;
		for (const shared_ptr<data::SignalBase>& s : session_.signalbases()) {
			if (!s->enabled())
				continue;

			QString s_name = s->name().toLower();
			s_name = s_name.replace(QRegExp("[-_.]"), " ");

			if (s->logic_data() &&
				((ch_name.contains(s_name)) || (s_name.contains(ch_name)))) {
				if (!match)
					match = s;
				else {
					// Only replace an existing match if it matches more characters
					int old_unmatched = ch_name.length() - match->name().length();
					int new_unmatched = ch_name.length() - s->name().length();
					if (abs(new_unmatched) < abs(old_unmatched))
						match = s;
				}
			}
		}

		if (match) {
			ch.assigned_signal = match.get();
			new_assignment = true;
		}
	}

	if (new_assignment) {
		logic_mux_data_invalid_ = true;
		stack_config_changed_ = true;
		commit_decoder_channels();
		channels_updated();
	}
}

void DecodeSignal::assign_signal(const uint16_t channel_id, const SignalBase *signal)
{
	for (decode::DecodeChannel& ch : channels_)
		if (ch.id == channel_id) {
			ch.assigned_signal = signal;
			logic_mux_data_invalid_ = true;
		}

	stack_config_changed_ = true;
	commit_decoder_channels();
	channels_updated();
	begin_decode();
}

int DecodeSignal::get_assigned_signal_count() const
{
	// Count all channels that have a signal assigned to them
	return count_if(channels_.begin(), channels_.end(),
		[](decode::DecodeChannel ch) { return ch.assigned_signal; });
}

void DecodeSignal::set_initial_pin_state(const uint16_t channel_id, const int init_state)
{
	for (decode::DecodeChannel& ch : channels_)
		if (ch.id == channel_id)
			ch.initial_pin_state = init_state;

	stack_config_changed_ = true;
	channels_updated();
	begin_decode();
}

double DecodeSignal::samplerate() const
{
	double result = 0;

	// TODO For now, we simply return the first samplerate that we have
	if (segments_.size() > 0)
		result = segments_.front().samplerate;

	return result;
}

const pv::util::Timestamp DecodeSignal::start_time() const
{
	pv::util::Timestamp result;

	// TODO For now, we simply return the first start time that we have
	if (segments_.size() > 0)
		result = segments_.front().start_time;

	return result;
}

int64_t DecodeSignal::get_working_sample_count(uint32_t segment_id) const
{
	// The working sample count is the highest sample number for
	// which all used signals have data available, so go through all
	// channels and use the lowest overall sample count of the segment

	int64_t count = std::numeric_limits<int64_t>::max();
	bool no_signals_assigned = true;

	for (const decode::DecodeChannel& ch : channels_)
		if (ch.assigned_signal) {
			no_signals_assigned = false;

			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();
			if (!logic_data || logic_data->logic_segments().empty())
				return 0;

			try {
				const shared_ptr<LogicSegment> segment = logic_data->logic_segments().at(segment_id);
				count = min(count, (int64_t)segment->get_sample_count());
			} catch (out_of_range&) {
				return 0;
			}
		}

	return (no_signals_assigned ? 0 : count);
}

int64_t DecodeSignal::get_decoded_sample_count(uint32_t segment_id,
	bool include_processing) const
{
	lock_guard<mutex> decode_lock(output_mutex_);

	int64_t result = 0;

	if (segment_id >= segments_.size())
		return result;

	if (include_processing)
		result = segments_[segment_id].samples_decoded_incl;
	else
		result = segments_[segment_id].samples_decoded_excl;

	return result;
}

vector<Row> DecodeSignal::get_rows(bool visible_only) const
{
	lock_guard<mutex> lock(output_mutex_);

	vector<Row> rows;

	for (const shared_ptr<decode::Decoder>& dec : stack_) {
		assert(dec);
		if (visible_only && !dec->shown())
			continue;

		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		int row_index = 0;
		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			rows.emplace_back(row_index++, dec.get());

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);
			rows.emplace_back(row_index++, dec.get(), ann_row);
		}
	}

	return rows;
}

uint64_t DecodeSignal::get_annotation_count(const decode::Row &row,
	uint32_t segment_id) const
{
	if (segment_id >= segments_.size())
		return 0;

	const DecodeSegment *segment = &(segments_.at(segment_id));
	const map<const decode::Row, decode::RowData> *rows =
		&(segment->annotation_rows);

	const auto iter = rows->find(row);
	if (iter != rows->end())
		return (*iter).second.get_annotation_count();

	return 0;
}

void DecodeSignal::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	const decode::Row &row, uint32_t segment_id, uint64_t start_sample,
	uint64_t end_sample) const
{
	lock_guard<mutex> lock(output_mutex_);

	if (segment_id >= segments_.size())
		return;

	const DecodeSegment *segment = &(segments_.at(segment_id));
	const map<const decode::Row, decode::RowData> *rows = &(segment->annotation_rows);

	const auto iter = rows->find(row);
	if (iter != rows->end())
		(*iter).second.get_annotation_subset(dest, start_sample, end_sample);
}

void DecodeSignal::get_annotation_subset(
	vector<pv::data::decode::Annotation> &dest,
	uint32_t segment_id, uint64_t start_sample, uint64_t end_sample) const
{
	// Note: We put all vectors and lists on the heap, not the stack

	const vector<Row> rows = get_rows();

	// Use forward_lists for faster merging
	forward_list<Annotation> *all_ann_list = new forward_list<Annotation>();

	for (const Row& row : rows) {
		vector<Annotation> *ann_vector = new vector<Annotation>();
		get_annotation_subset(*ann_vector, row, segment_id, start_sample, end_sample);

		forward_list<Annotation> *ann_list =
			new forward_list<Annotation>(ann_vector->begin(), ann_vector->end());
		delete ann_vector;

		all_ann_list->merge(*ann_list);
		delete ann_list;
	}

	move(all_ann_list->begin(), all_ann_list->end(), back_inserter(dest));
	delete all_ann_list;
}

uint32_t DecodeSignal::get_binary_data_chunk_count(uint32_t segment_id,
	const Decoder* dec, uint32_t bin_class_id) const
{
	if (segments_.size() == 0)
		return 0;

	try {
		const DecodeSegment *segment = &(segments_.at(segment_id));

		for (const DecodeBinaryClass& bc : segment->binary_classes)
			if ((bc.decoder == dec) && (bc.info->bin_class_id == bin_class_id))
				return bc.chunks.size();
	} catch (out_of_range&) {
		// Do nothing
	}

	return 0;
}

void DecodeSignal::get_binary_data_chunk(uint32_t segment_id,
	const  Decoder* dec, uint32_t bin_class_id, uint32_t chunk_id,
	const vector<uint8_t> **dest, uint64_t *size)
{
	try {
		const DecodeSegment *segment = &(segments_.at(segment_id));

		for (const DecodeBinaryClass& bc : segment->binary_classes)
			if ((bc.decoder == dec) && (bc.info->bin_class_id == bin_class_id)) {
				if (dest) *dest = &(bc.chunks.at(chunk_id).data);
				if (size) *size = bc.chunks.at(chunk_id).data.size();
				return;
			}
	} catch (out_of_range&) {
		// Do nothing
	}
}

void DecodeSignal::get_merged_binary_data_chunks_by_sample(uint32_t segment_id,
	const Decoder* dec, uint32_t bin_class_id, uint64_t start_sample,
	uint64_t end_sample, vector<uint8_t> *dest) const
{
	assert(dest != nullptr);

	try {
		const DecodeSegment *segment = &(segments_.at(segment_id));

		const DecodeBinaryClass* bin_class = nullptr;
		for (const DecodeBinaryClass& bc : segment->binary_classes)
			if ((bc.decoder == dec) && (bc.info->bin_class_id == bin_class_id))
				bin_class = &bc;

		// Determine overall size before copying to resize dest vector only once
		uint64_t size = 0;
		uint64_t matches = 0;
		for (const DecodeBinaryDataChunk& chunk : bin_class->chunks)
			if ((chunk.sample >= start_sample) && (chunk.sample < end_sample)) {
				size += chunk.data.size();
				matches++;
			}
		dest->resize(size);

		uint64_t offset = 0;
		uint64_t matches2 = 0;
		for (const DecodeBinaryDataChunk& chunk : bin_class->chunks)
			if ((chunk.sample >= start_sample) && (chunk.sample < end_sample)) {
				memcpy(dest->data() + offset, chunk.data.data(), chunk.data.size());
				offset += chunk.data.size();
				matches2++;

				// Make sure we don't overwrite memory if the array grew in the meanwhile
				if (matches2 == matches)
					break;
			}
	} catch (out_of_range&) {
		// Do nothing
	}
}

void DecodeSignal::get_merged_binary_data_chunks_by_offset(uint32_t segment_id,
	const data::decode::Decoder* dec, uint32_t bin_class_id, uint64_t start,
	uint64_t end, vector<uint8_t> *dest) const
{
	assert(dest != nullptr);

	try {
		const DecodeSegment *segment = &(segments_.at(segment_id));

		const DecodeBinaryClass* bin_class = nullptr;
		for (const DecodeBinaryClass& bc : segment->binary_classes)
			if ((bc.decoder == dec) && (bc.info->bin_class_id == bin_class_id))
				bin_class = &bc;

		// Determine overall size before copying to resize dest vector only once
		uint64_t size = 0;
		uint64_t offset = 0;
		for (const DecodeBinaryDataChunk& chunk : bin_class->chunks) {
			if (offset >= start)
				size += chunk.data.size();
			offset += chunk.data.size();
			if (offset >= end)
				break;
		}
		dest->resize(size);

		offset = 0;
		uint64_t dest_offset = 0;
		for (const DecodeBinaryDataChunk& chunk : bin_class->chunks) {
			if (offset >= start) {
				memcpy(dest->data() + dest_offset, chunk.data.data(), chunk.data.size());
				dest_offset += chunk.data.size();
			}
			offset += chunk.data.size();
			if (offset >= end)
				break;
		}
	} catch (out_of_range&) {
		// Do nothing
	}
}

const DecodeBinaryClass* DecodeSignal::get_binary_data_class(uint32_t segment_id,
	const data::decode::Decoder* dec, uint32_t bin_class_id) const
{
	try {
		const DecodeSegment *segment = &(segments_.at(segment_id));

		for (const DecodeBinaryClass& bc : segment->binary_classes)
			if ((bc.decoder == dec) && (bc.info->bin_class_id == bin_class_id))
				return &bc;
	} catch (out_of_range&) {
		// Do nothing
	}

	return nullptr;
}

void DecodeSignal::save_settings(QSettings &settings) const
{
	SignalBase::save_settings(settings);

	settings.setValue("decoders", (int)(stack_.size()));

	// Save decoder stack
	int decoder_idx = 0;
	for (const shared_ptr<decode::Decoder>& decoder : stack_) {
		settings.beginGroup("decoder" + QString::number(decoder_idx++));

		settings.setValue("id", decoder->decoder()->id);
		settings.setValue("shown", decoder->shown());

		// Save decoder options
		const map<string, GVariant*>& options = decoder->options();

		settings.setValue("options", (int)options.size());

		// Note: decode::Decoder::options() returns only the options
		// that differ from the default. See binding::Decoder::getter()
		int i = 0;
		for (auto& option : options) {
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
			[&](decode::DecodeChannel ch) { return ch.id == channel_id; });

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
				decoder->show(settings.value("shown", true).toBool());

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
			[&](decode::DecodeChannel ch) { return ch.id == channel_id; });

		if (channel == channels_.end()) {
			qDebug() << "ERROR: Non-existant channel index:" << channel_id;
			continue;
		}

		settings.beginGroup("channel" + QString::number(channel_id));

		QString assigned_signal_name = settings.value("assigned_signal_name").toString();

		for (const shared_ptr<data::SignalBase>& signal : signalbases)
			if (signal->name() == assigned_signal_name)
				channel->assigned_signal = signal.get();

		channel->initial_pin_state = settings.value("initial_pin_state").toInt();

		settings.endGroup();
	}

	// Update the internal structures
	stack_config_changed_ = true;
	update_channel_list();
	commit_decoder_channels();

	begin_decode();
}

void DecodeSignal::set_error_message(QString msg)
{
	error_message_ = msg;
	// TODO Emulate noquote()
	qDebug().nospace() << name() << ": " << msg;
}

uint32_t DecodeSignal::get_input_segment_count() const
{
	uint64_t count = std::numeric_limits<uint64_t>::max();
	bool no_signals_assigned = true;

	for (const decode::DecodeChannel& ch : channels_)
		if (ch.assigned_signal) {
			no_signals_assigned = false;

			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();
			if (!logic_data || logic_data->logic_segments().empty())
				return 0;

			// Find the min value of all segment counts
			if ((uint64_t)(logic_data->logic_segments().size()) < count)
				count = logic_data->logic_segments().size();
		}

	return (no_signals_assigned ? 0 : count);
}

uint32_t DecodeSignal::get_input_samplerate(uint32_t segment_id) const
{
	double samplerate = 0;

	for (const decode::DecodeChannel& ch : channels_)
		if (ch.assigned_signal) {
			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();
			if (!logic_data || logic_data->logic_segments().empty())
				continue;

			try {
				const shared_ptr<LogicSegment> segment = logic_data->logic_segments().at(segment_id);
				samplerate = segment->samplerate();
			} catch (out_of_range&) {
				// Do nothing
			}
			break;
		}

	return samplerate;
}

void DecodeSignal::update_channel_list()
{
	vector<decode::DecodeChannel> prev_channels = channels_;
	channels_.clear();

	uint16_t id = 0;

	// Copy existing entries, create new as needed
	for (shared_ptr<Decoder>& decoder : stack_) {
		const srd_decoder* srd_d = decoder->decoder();
		const GSList *l;

		// Mandatory channels
		for (l = srd_d->channels; l; l = l->next) {
			const struct srd_channel *const pdch = (struct srd_channel *)l->data;
			bool ch_added = false;

			// Copy but update ID if this channel was in the list before
			for (decode::DecodeChannel& ch : prev_channels)
				if (ch.pdch_ == pdch) {
					ch.id = id++;
					channels_.push_back(ch);
					ch_added = true;
					break;
				}

			if (!ch_added) {
				// Create new entry without a mapped signal
				decode::DecodeChannel ch = {id++, 0, false, nullptr,
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
			for (decode::DecodeChannel& ch : prev_channels)
				if (ch.pdch_ == pdch) {
					ch.id = id++;
					channels_.push_back(ch);
					ch_added = true;
					break;
				}

			if (!ch_added) {
				// Create new entry without a mapped signal
				decode::DecodeChannel ch = {id++, 0, true, nullptr,
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
			const decode::DecodeChannel& p_ch = prev_channels[i];
			const decode::DecodeChannel& ch = channels_[i];

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
		vector<decode::DecodeChannel*> channel_list;

		for (decode::DecodeChannel& ch : channels_)
			if (ch.decoder_ == dec)
				channel_list.push_back(&ch);

		dec->set_channels(channel_list);
	}

	// Channel bit IDs must be in sync with the channel's apperance in channels_
	int id = 0;
	for (decode::DecodeChannel& ch : channels_)
		if (ch.assigned_signal)
			ch.bit_id = id++;
}

void DecodeSignal::mux_logic_samples(uint32_t segment_id, const int64_t start, const int64_t end)
{
	// Enforce end to be greater than start
	if (end <= start)
		return;

	// Fetch the channel segments and their data
	vector<shared_ptr<LogicSegment> > segments;
	vector<const uint8_t*> signal_data;
	vector<uint8_t> signal_in_bytepos;
	vector<uint8_t> signal_in_bitpos;

	for (decode::DecodeChannel& ch : channels_)
		if (ch.assigned_signal) {
			const shared_ptr<Logic> logic_data = ch.assigned_signal->logic_data();

			shared_ptr<LogicSegment> segment;
			try {
				segment = logic_data->logic_segments().at(segment_id);
			} catch (out_of_range&) {
				qDebug() << "Muxer error for" << name() << ":" << ch.assigned_signal->name() \
					<< "has no logic segment" << segment_id;
				return;
			}
			segments.push_back(segment);

			uint8_t* data = new uint8_t[(end - start) * segment->unit_size()];
			segment->get_samples(start, end, data);
			signal_data.push_back(data);

			const int bitpos = ch.assigned_signal->logic_bit_index();
			signal_in_bytepos.push_back(bitpos / 8);
			signal_in_bitpos.push_back(bitpos % 8);
		}


	shared_ptr<LogicSegment> output_segment;
	try {
		output_segment = logic_mux_data_->logic_segments().at(segment_id);
	} catch (out_of_range&) {
		qDebug() << "Muxer error for" << name() << ": no logic mux segment" \
			<< segment_id << "in mux_logic_samples(), mux segments size is" \
			<< logic_mux_data_->logic_segments().size();
		return;
	}

	// Perform the muxing of signal data into the output data
	uint8_t* output = new uint8_t[(end - start) * output_segment->unit_size()];
	unsigned int signal_count = signal_data.size();

	for (int64_t sample_cnt = 0; !logic_mux_interrupt_ && (sample_cnt < (end - start));
		sample_cnt++) {

		int bitpos = 0;
		uint8_t bytepos = 0;

		const int out_sample_pos = sample_cnt * output_segment->unit_size();
		for (unsigned int i = 0; i < output_segment->unit_size(); i++)
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

	output_segment->append_payload(output, (end - start) * output_segment->unit_size());
	delete[] output;

	for (const uint8_t* data : signal_data)
		delete[] data;
}

void DecodeSignal::logic_mux_proc()
{
	uint32_t segment_id = 0;

	assert(logic_mux_data_);

	// Create initial logic mux segment
	shared_ptr<LogicSegment> output_segment =
		make_shared<LogicSegment>(*logic_mux_data_, segment_id,
			logic_mux_unit_size_, 0);
	logic_mux_data_->push_segment(output_segment);

	output_segment->set_samplerate(get_input_samplerate(0));

	do {
		const uint64_t input_sample_count = get_working_sample_count(segment_id);
		const uint64_t output_sample_count = output_segment->get_sample_count();

		const uint64_t samples_to_process =
			(input_sample_count > output_sample_count) ?
			(input_sample_count - output_sample_count) : 0;

		// Process the samples if necessary...
		if (samples_to_process > 0) {
			const uint64_t unit_size = output_segment->unit_size();
			const uint64_t chunk_sample_count = DecodeChunkLength / unit_size;

			uint64_t processed_samples = 0;
			do {
				const uint64_t start_sample = output_sample_count + processed_samples;
				const uint64_t sample_count =
					min(samples_to_process - processed_samples,	chunk_sample_count);

				mux_logic_samples(segment_id, start_sample, start_sample + sample_count);
				processed_samples += sample_count;

				// ...and process the newly muxed logic data
				decode_input_cond_.notify_one();
			} while (!logic_mux_interrupt_ && (processed_samples < samples_to_process));
		}

		if (samples_to_process == 0) {
			// TODO Optimize this by caching the input segment count and only
			// querying it when the cached value was reached
			if (segment_id < get_input_segment_count() - 1) {
				// Process next segment
				segment_id++;

				output_segment =
					make_shared<LogicSegment>(*logic_mux_data_, segment_id,
						logic_mux_unit_size_, 0);
				logic_mux_data_->push_segment(output_segment);

				output_segment->set_samplerate(get_input_samplerate(segment_id));

			} else {
				// All segments have been processed
				logic_mux_data_invalid_ = false;

				// Wait for more input
				unique_lock<mutex> logic_mux_lock(logic_mux_mutex_);
				logic_mux_cond_.wait(logic_mux_lock);
			}
		}

	} while (!logic_mux_interrupt_);
}

void DecodeSignal::decode_data(
	const int64_t abs_start_samplenum, const int64_t sample_count,
	const shared_ptr<LogicSegment> input_segment)
{
	const int64_t unit_size = input_segment->unit_size();
	const int64_t chunk_sample_count = DecodeChunkLength / unit_size;

	for (int64_t i = abs_start_samplenum;
		error_message_.isEmpty() && !decode_interrupt_ &&
			(i < (abs_start_samplenum + sample_count));
		i += chunk_sample_count) {

		const int64_t chunk_end = min(i + chunk_sample_count,
			abs_start_samplenum + sample_count);

		{
			lock_guard<mutex> lock(output_mutex_);
			// Update the sample count showing the samples including currently processed ones
			segments_.at(current_segment_id_).samples_decoded_incl = chunk_end;
		}

		int64_t data_size = (chunk_end - i) * unit_size;
		uint8_t* chunk = new uint8_t[data_size];
		input_segment->get_samples(i, chunk_end, chunk);

		if (srd_session_send(srd_session_, i, chunk_end, chunk,
				data_size, unit_size) != SRD_OK)
			set_error_message(tr("Decoder reported an error"));

		delete[] chunk;

		{
			lock_guard<mutex> lock(output_mutex_);
			// Now that all samples are processed, the exclusive sample count catches up
			segments_.at(current_segment_id_).samples_decoded_excl = chunk_end;
		}

		// Notify the frontend that we processed some data and
		// possibly have new annotations as well
		new_annotations();

		if (decode_paused_) {
			unique_lock<mutex> pause_wait_lock(decode_pause_mutex_);
			decode_pause_cond_.wait(pause_wait_lock);
		}
	}
}

void DecodeSignal::decode_proc()
{
	current_segment_id_ = 0;

	// If there is no input data available yet, wait until it is or we're interrupted
	if (logic_mux_data_->logic_segments().size() == 0) {
		unique_lock<mutex> input_wait_lock(input_mutex_);
		decode_input_cond_.wait(input_wait_lock);
	}

	if (decode_interrupt_)
		return;

	shared_ptr<LogicSegment> input_segment = logic_mux_data_->logic_segments().front();
	assert(input_segment);

	// Create the initial segment and set its sample rate so that we can pass it to SRD
	create_decode_segment();
	segments_.at(current_segment_id_).samplerate = input_segment->samplerate();
	segments_.at(current_segment_id_).start_time = input_segment->start_time();

	start_srd_session();

	uint64_t sample_count = 0;
	uint64_t abs_start_samplenum = 0;
	do {
		// Keep processing new samples until we exhaust the input data
		do {
			lock_guard<mutex> input_lock(input_mutex_);
			sample_count = input_segment->get_sample_count() - abs_start_samplenum;

			if (sample_count > 0) {
				decode_data(abs_start_samplenum, sample_count, input_segment);
				abs_start_samplenum += sample_count;
			}
		} while (error_message_.isEmpty() && (sample_count > 0) && !decode_interrupt_);

		if (error_message_.isEmpty() && !decode_interrupt_ && sample_count == 0) {
			if (current_segment_id_ < logic_mux_data_->logic_segments().size() - 1) {
				// Process next segment
				current_segment_id_++;

				try {
					input_segment = logic_mux_data_->logic_segments().at(current_segment_id_);
				} catch (out_of_range&) {
					qDebug() << "Decode error for" << name() << ": no logic mux segment" \
						<< current_segment_id_ << "in decode_proc(), mux segments size is" \
						<< logic_mux_data_->logic_segments().size();
					return;
				}
				abs_start_samplenum = 0;

				// Create the next segment and set its metadata
				create_decode_segment();
				segments_.at(current_segment_id_).samplerate = input_segment->samplerate();
				segments_.at(current_segment_id_).start_time = input_segment->start_time();

				// Reset decoder state but keep the decoder stack intact
				terminate_srd_session();
			} else {
				// All segments have been processed
				decode_finished();

				// Wait for new input data or an interrupt was requested
				unique_lock<mutex> input_wait_lock(input_mutex_);
				decode_input_cond_.wait(input_wait_lock);
			}
		}
	} while (error_message_.isEmpty() && !decode_interrupt_);

	// Potentially reap decoders when the application no longer is
	// interested in their (pending) results.
	if (decode_interrupt_)
		terminate_srd_session();
}

void DecodeSignal::start_srd_session()
{
	// If there were stack changes, the session has been destroyed by now, so if
	// it hasn't been destroyed, we can just reset and re-use it
	if (srd_session_) {
		// When a decoder stack was created before, re-use it
		// for the next stream of input data, after terminating
		// potentially still executing operations, and resetting
		// internal state. Skip the rather expensive (teardown
		// and) construction of another decoder stack.

		// TODO Reduce redundancy, use a common code path for
		// the meta/start sequence?
		terminate_srd_session();

		// Metadata is cleared also, so re-set it
		uint64_t samplerate = 0;
		if (segments_.size() > 0)
			samplerate = segments_.at(current_segment_id_).samplerate;
		if (samplerate)
			srd_session_metadata_set(srd_session_, SRD_CONF_SAMPLERATE,
				g_variant_new_uint64(samplerate));
		for (const shared_ptr<decode::Decoder>& dec : stack_)
			dec->apply_all_options();
		srd_session_start(srd_session_);

		return;
	}

	// Create the session
	srd_session_new(&srd_session_);
	assert(srd_session_);

	// Create the decoders
	srd_decoder_inst *prev_di = nullptr;
	for (const shared_ptr<decode::Decoder>& dec : stack_) {
		srd_decoder_inst *const di = dec->create_decoder_inst(srd_session_);

		if (!di) {
			set_error_message(tr("Failed to create decoder instance"));
			srd_session_destroy(srd_session_);
			srd_session_ = nullptr;
			return;
		}

		if (prev_di)
			srd_inst_stack(srd_session_, prev_di, di);

		prev_di = di;
	}

	// Start the session
	if (segments_.size() > 0)
		srd_session_metadata_set(srd_session_, SRD_CONF_SAMPLERATE,
			g_variant_new_uint64(segments_.at(current_segment_id_).samplerate));

	srd_pd_output_callback_add(srd_session_, SRD_OUTPUT_ANN,
		DecodeSignal::annotation_callback, this);

	srd_pd_output_callback_add(srd_session_, SRD_OUTPUT_BINARY,
		DecodeSignal::binary_callback, this);

	srd_session_start(srd_session_);

	// We just recreated the srd session, so all stack changes are applied now
	stack_config_changed_ = false;
}

void DecodeSignal::terminate_srd_session()
{
	// Call the "terminate and reset" routine for the decoder stack
	// (if available). This does not harm those stacks which already
	// have completed their operation, and reduces response time for
	// those stacks which still are processing data while the
	// application no longer wants them to.
	if (srd_session_) {
		srd_session_terminate_reset(srd_session_);

		// Metadata is cleared also, so re-set it
		uint64_t samplerate = 0;
		if (segments_.size() > 0)
			samplerate = segments_.at(current_segment_id_).samplerate;
		if (samplerate)
			srd_session_metadata_set(srd_session_, SRD_CONF_SAMPLERATE,
				g_variant_new_uint64(samplerate));
		for (const shared_ptr<decode::Decoder>& dec : stack_)
			dec->apply_all_options();
	}
}

void DecodeSignal::stop_srd_session()
{
	if (srd_session_) {
		// Destroy the session
		srd_session_destroy(srd_session_);
		srd_session_ = nullptr;

		// Mark the decoder instances as non-existant since they were deleted
		for (const shared_ptr<decode::Decoder>& dec : stack_)
			dec->invalidate_decoder_inst();
	}
}

void DecodeSignal::connect_input_notifiers()
{
	// Disconnect the notification slot from the previous set of signals
	disconnect(this, SLOT(on_data_cleared()));
	disconnect(this, SLOT(on_data_received()));

	// Connect the currently used signals to our slot
	for (decode::DecodeChannel& ch : channels_) {
		if (!ch.assigned_signal)
			continue;

		const data::SignalBase *signal = ch.assigned_signal;
		connect(signal, SIGNAL(samples_cleared()),
			this, SLOT(on_data_cleared()));
		connect(signal, SIGNAL(samples_added(uint64_t, uint64_t, uint64_t)),
			this, SLOT(on_data_received()));
	}
}

void DecodeSignal::create_decode_segment()
{
	// Create annotation segment
	segments_.emplace_back(DecodeSegment());

	// Add annotation classes
	for (const shared_ptr<decode::Decoder>& dec : stack_) {
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		int row_index = 0;
		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			(segments_.back().annotation_rows)[Row(row_index++, dec.get())] =
				decode::RowData();

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next) {
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);

			const Row row(row_index++, dec.get(), ann_row);

			// Add a new empty row data object
			(segments_.back().annotation_rows)[row] = decode::RowData();
		}
	}

	// Prepare our binary output classes
	for (const shared_ptr<decode::Decoder>& dec : stack_) {
		uint32_t n = dec->get_binary_class_count();

		for (uint32_t i = 0; i < n; i++)
			segments_.back().binary_classes.push_back(
				{dec.get(), dec->get_binary_class(i), deque<DecodeBinaryDataChunk>()});
	}
}

void DecodeSignal::annotation_callback(srd_proto_data *pdata, void *decode_signal)
{
	assert(pdata);
	assert(decode_signal);

	DecodeSignal *const ds = (DecodeSignal*)decode_signal;
	assert(ds);

	if (ds->decode_interrupt_)
		return;

	lock_guard<mutex> lock(ds->output_mutex_);

	// Get the decoder and the annotation data
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

	const srd_proto_data_annotation *const pda = (const srd_proto_data_annotation*)pdata->data;
	assert(pda);

	// Find the row
	auto row_iter = ds->segments_.at(ds->current_segment_id_).annotation_rows.end();

	// Try finding a better row match than the default by looking up the sub-row of this class
	const auto format = pda->ann_class;
	const auto r = ds->class_rows_.find(make_pair(decc, format));
	if (r != ds->class_rows_.end())
		row_iter = ds->segments_.at(ds->current_segment_id_).annotation_rows.find((*r).second);
	else {
		// Failing that, use the decoder as a key
		for (const shared_ptr<decode::Decoder>& dec : ds->decoder_stack())
			if (dec->decoder() == decc)
				row_iter = ds->segments_.at(ds->current_segment_id_).annotation_rows.find(Row(0, dec.get()));
	}

	if (row_iter == ds->segments_.at(ds->current_segment_id_).annotation_rows.end()) {
		qDebug() << "Unexpected annotation: decoder = " << decc <<
			", format = " << format;
		assert(false);
		return;
	}

	// Add the annotation
	(*row_iter).second.emplace_annotation(pdata, &((*row_iter).first));
}

void DecodeSignal::binary_callback(srd_proto_data *pdata, void *decode_signal)
{
	assert(pdata);
	assert(decode_signal);

	DecodeSignal *const ds = (DecodeSignal*)decode_signal;
	assert(ds);

	if (ds->decode_interrupt_)
		return;

	// Get the decoder and the binary data
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

	const srd_proto_data_binary *const pdb = (const srd_proto_data_binary*)pdata->data;
	assert(pdb);

	// Find the matching DecodeBinaryClass
	DecodeSegment* segment = &(ds->segments_.at(ds->current_segment_id_));

	DecodeBinaryClass* bin_class = nullptr;
	for (DecodeBinaryClass& bc : segment->binary_classes)
		if ((bc.decoder->decoder() == decc) && (bc.info->bin_class_id == (uint32_t)pdb->bin_class))
			bin_class = &bc;

	if (!bin_class) {
		qWarning() << "Could not find valid DecodeBinaryClass in segment" <<
				ds->current_segment_id_ << "for binary class ID" << pdb->bin_class <<
				", segment only knows" << segment->binary_classes.size() << "classes";
		return;
	}

	// Add the data chunk
	bin_class->chunks.emplace_back();
	DecodeBinaryDataChunk* chunk = &(bin_class->chunks.back());

	chunk->sample = pdata->start_sample;
	chunk->data.resize(pdb->size);
	memcpy(chunk->data.data(), pdb->data, pdb->size);

	// Find decoder class instance
	Decoder* dec = nullptr;
	for (const shared_ptr<decode::Decoder>& d : ds->decoder_stack())
		if (d->decoder() == decc) {
			dec = d.get();
			break;
		}

	ds->new_binary_data(ds->current_segment_id_, (void*)dec, pdb->bin_class);
}

void DecodeSignal::on_capture_state_changed(int state)
{
	// If a new acquisition was started, we need to start decoding from scratch
	if (state == Session::Running) {
		logic_mux_data_invalid_ = true;
		begin_decode();
	}
}

void DecodeSignal::on_data_cleared()
{
	reset_decode();
}

void DecodeSignal::on_data_received()
{
	// If we detected a lack of input data when trying to start decoding,
	// we have set an error message. Only try again if we now have data
	// to work with
	if ((!error_message_.isEmpty()) && (get_input_segment_count() == 0))
		return;

	if (!logic_mux_thread_.joinable())
		begin_decode();
	else
		logic_mux_cond_.notify_one();
}

} // namespace data
} // namespace pv
