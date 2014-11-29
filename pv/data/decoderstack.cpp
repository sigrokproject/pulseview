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

#include <stdexcept>

#include <QDebug>

#include "decoderstack.hpp"

#include <pv/data/logic.hpp>
#include <pv/data/logicsnapshot.hpp>
#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/annotation.hpp>
#include <pv/session.hpp>
#include <pv/view/logicsignal.hpp>

using std::lock_guard;
using std::mutex;
using boost::optional;
using std::unique_lock;
using std::deque;
using std::make_pair;
using std::max;
using std::min;
using std::list;
using std::map;
using std::pair;
using std::shared_ptr;
using std::vector;

using namespace pv::data::decode;

namespace pv {
namespace data {

const double DecoderStack::DecodeMargin = 1.0;
const double DecoderStack::DecodeThreshold = 0.2;
const int64_t DecoderStack::DecodeChunkLength = 4096;
const unsigned int DecoderStack::DecodeNotifyPeriod = 65536;

mutex DecoderStack::global_decode_mutex_;

DecoderStack::DecoderStack(pv::Session &session,
	const srd_decoder *const dec) :
	session_(session),
	start_time_(0),
	samplerate_(0),
	sample_count_(0),
	frame_complete_(false),
	samples_decoded_(0)
{
	connect(&session_, SIGNAL(frame_began()),
		this, SLOT(on_new_frame()));
	connect(&session_, SIGNAL(data_received()),
		this, SLOT(on_data_received()));
	connect(&session_, SIGNAL(frame_ended()),
		this, SLOT(on_frame_ended()));

	stack_.push_back(shared_ptr<decode::Decoder>(
		new decode::Decoder(dec)));
}

DecoderStack::~DecoderStack()
{
	if (decode_thread_.joinable()) {
		interrupt_ = true;
		input_cond_.notify_one();
		decode_thread_.join();
	}
}

const std::list< std::shared_ptr<decode::Decoder> >&
DecoderStack::stack() const
{
	return stack_;
}

void DecoderStack::push(std::shared_ptr<decode::Decoder> decoder)
{
	assert(decoder);
	stack_.push_back(decoder);
}

void DecoderStack::remove(int index)
{
	assert(index >= 0);
	assert(index < (int)stack_.size());

	// Find the decoder in the stack
	auto iter = stack_.begin();
	for(int i = 0; i < index; i++, iter++)
		assert(iter != stack_.end());

	// Delete the element
	stack_.erase(iter);
}

double DecoderStack::samplerate() const
{
	return samplerate_;
}

double DecoderStack::start_time() const
{
	return start_time_;
}

int64_t DecoderStack::samples_decoded() const
{
	lock_guard<mutex> decode_lock(output_mutex_);
	return samples_decoded_;
}

std::vector<Row> DecoderStack::get_visible_rows() const
{
	lock_guard<mutex> lock(output_mutex_);

	vector<Row> rows;

	for (const shared_ptr<decode::Decoder> &dec : stack_)
	{
		assert(dec);
		if (!dec->shown())
			continue;

		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			rows.push_back(Row(decc));

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next)
		{
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);
			rows.push_back(Row(decc, ann_row));
		}
	}

	return rows;
}

void DecoderStack::get_annotation_subset(
	std::vector<pv::data::decode::Annotation> &dest,
	const Row &row, uint64_t start_sample,
	uint64_t end_sample) const
{
	lock_guard<mutex> lock(output_mutex_);

	const auto iter = rows_.find(row);
	if (iter != rows_.end())
		(*iter).second.get_annotation_subset(dest,
			start_sample, end_sample);
}

QString DecoderStack::error_message()
{
	lock_guard<mutex> lock(output_mutex_);
	return error_message_;
}

void DecoderStack::clear()
{
	sample_count_ = 0;
	frame_complete_ = false;
	samples_decoded_ = 0;
	error_message_ = QString();
	rows_.clear();
	class_rows_.clear();
}

void DecoderStack::begin_decode()
{
	shared_ptr<pv::view::LogicSignal> logic_signal;
	shared_ptr<pv::data::Logic> data;

	if (decode_thread_.joinable()) {
		interrupt_ = true;
		input_cond_.notify_one();
		decode_thread_.join();
	}

	clear();

	// Check that all decoders have the required channels
	for (const shared_ptr<decode::Decoder> &dec : stack_)
		if (!dec->have_required_channels()) {
			error_message_ = tr("One or more required channels "
				"have not been specified");
			return;
		}

	// Add classes
	for (const shared_ptr<decode::Decoder> &dec : stack_)
	{
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			rows_[Row(decc)] = decode::RowData();

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next)
		{
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

	// We get the logic data of the first channel in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
	for (const shared_ptr<decode::Decoder> &dec : stack_)
		if (dec && !dec->channels().empty() &&
			((logic_signal = (*dec->channels().begin()).second)) &&
			((data = logic_signal->logic_data())))
			break;

	if (!data)
		return;

	// Check we have a snapshot of data
	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		data->logic_snapshots();
	if (snapshots.empty())
		return;
	snapshot_ = snapshots.front();

	// Get the samplerate and start time
	start_time_ = snapshot_->start_time();
	samplerate_ = data->samplerate();
	if (samplerate_ == 0.0)
		samplerate_ = 1.0;

	interrupt_ = false;
	decode_thread_ = std::thread(&DecoderStack::decode_proc, this);
}

uint64_t DecoderStack::get_max_sample_count() const
{
	uint64_t max_sample_count = 0;

	for (auto i = rows_.cbegin(); i != rows_.end(); i++)
		max_sample_count = max(max_sample_count,
			(*i).second.get_max_sample());

	return max_sample_count;
}

optional<int64_t> DecoderStack::wait_for_data() const
{
	unique_lock<mutex> input_lock(input_mutex_);
	while(!interrupt_ && !frame_complete_ &&
		samples_decoded_ >= sample_count_)
		input_cond_.wait(input_lock);
	return boost::make_optional(!interrupt_ &&
		(samples_decoded_ < sample_count_ || !frame_complete_),
		sample_count_);
}

void DecoderStack::decode_data(
	const int64_t sample_count, const unsigned int unit_size,
	srd_session *const session)
{
	uint8_t chunk[DecodeChunkLength];

	const unsigned int chunk_sample_count =
		DecodeChunkLength / snapshot_->unit_size();

	for (int64_t i = 0; !interrupt_ && i < sample_count;
		i += chunk_sample_count)
	{
		lock_guard<mutex> decode_lock(global_decode_mutex_);

		const int64_t chunk_end = min(
			i + chunk_sample_count, sample_count);
		snapshot_->get_samples(chunk, i, chunk_end);

		if (srd_session_send(session, i, i + sample_count, chunk,
				(chunk_end - i) * unit_size) != SRD_OK) {
			error_message_ = tr("Decoder reported an error");
			break;
		}

		{
			lock_guard<mutex> lock(output_mutex_);
			samples_decoded_ = chunk_end;
		}

		if (i % DecodeNotifyPeriod == 0)
			new_decode_data();
	}

	new_decode_data();
}

void DecoderStack::decode_proc()
{
	optional<int64_t> sample_count;
	srd_session *session;
	srd_decoder_inst *prev_di = NULL;

	assert(snapshot_);

	// Create the session
	srd_session_new(&session);
	assert(session);

	// Create the decoders
	const unsigned int unit_size = snapshot_->unit_size();

	for (const shared_ptr<decode::Decoder> &dec : stack_)
	{
		srd_decoder_inst *const di = dec->create_decoder_inst(session, unit_size);

		if (!di)
		{
			error_message_ = tr("Failed to create decoder instance");
			srd_session_destroy(session);
			return;
		}

		if (prev_di)
			srd_inst_stack (session, prev_di, di);

		prev_di = di;
	}

	// Get the intial sample count
	{
		unique_lock<mutex> input_lock(input_mutex_);
		sample_count = sample_count_ = snapshot_->get_sample_count();
	}

	// Start the session
	srd_session_metadata_set(session, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64((uint64_t)samplerate_));

	srd_pd_output_callback_add(session, SRD_OUTPUT_ANN,
		DecoderStack::annotation_callback, this);

	srd_session_start(session);

	do {
		decode_data(*sample_count, unit_size, session);
	} while(error_message_.isEmpty() && (sample_count = wait_for_data()));

	// Destroy the session
	srd_session_destroy(session);
}

void DecoderStack::annotation_callback(srd_proto_data *pdata, void *decoder)
{
	assert(pdata);
	assert(decoder);

	DecoderStack *const d = (DecoderStack*)decoder;
	assert(d);

	lock_guard<mutex> lock(d->output_mutex_);

	const Annotation a(pdata);

	// Find the row
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

	auto row_iter = d->rows_.end();

	// Try looking up the sub-row of this class
	const auto r = d->class_rows_.find(make_pair(decc, a.format()));
	if (r != d->class_rows_.end())
		row_iter = d->rows_.find((*r).second);
	else
	{
		// Failing that, use the decoder as a key
		row_iter = d->rows_.find(Row(decc));	
	}

	assert(row_iter != d->rows_.end());
	if (row_iter == d->rows_.end()) {
		qDebug() << "Unexpected annotation: decoder = " << decc <<
			", format = " << a.format();
		assert(0);
		return;
	}

	// Add the annotation
	(*row_iter).second.push_annotation(a);
}

void DecoderStack::on_new_frame()
{
	begin_decode();
}

void DecoderStack::on_data_received()
{
	{
		unique_lock<mutex> lock(input_mutex_);
		if (snapshot_)
			sample_count_ = snapshot_->get_sample_count();
	}
	input_cond_.notify_one();
}

void DecoderStack::on_frame_ended()
{
	{
		unique_lock<mutex> lock(input_mutex_);
		if (snapshot_)
			frame_complete_ = true;
	}
	input_cond_.notify_one();
}

} // namespace data
} // namespace pv
