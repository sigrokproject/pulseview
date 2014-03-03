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

#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include <stdexcept>

#include <QDebug>

#include "decoderstack.h"

#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/data/decode/decoder.h>
#include <pv/data/decode/annotation.h>
#include <pv/sigsession.h>
#include <pv/view/logicsignal.h>

using boost::lock_guard;
using boost::mutex;
using boost::optional;
using boost::shared_ptr;
using boost::unique_lock;
using std::deque;
using std::make_pair;
using std::max;
using std::min;
using std::list;
using std::map;
using std::pair;
using std::vector;

using namespace pv::data::decode;

namespace pv {
namespace data {

const double DecoderStack::DecodeMargin = 1.0;
const double DecoderStack::DecodeThreshold = 0.2;
const int64_t DecoderStack::DecodeChunkLength = 4096;

mutex DecoderStack::_global_decode_mutex;

DecoderStack::DecoderStack(pv::SigSession &session,
	const srd_decoder *const dec) :
	_session(session),
	_sample_count(0),
	_frame_complete(false),
	_samples_decoded(0)
{
	connect(&_session, SIGNAL(frame_began()),
		this, SLOT(on_new_frame()));
	connect(&_session, SIGNAL(data_received()),
		this, SLOT(on_data_received()));
	connect(&_session, SIGNAL(frame_ended()),
		this, SLOT(on_frame_ended()));

	_stack.push_back(shared_ptr<decode::Decoder>(
		new decode::Decoder(dec)));
}

DecoderStack::~DecoderStack()
{
	if (_decode_thread.joinable()) {
		_decode_thread.interrupt();
		_decode_thread.join();
	}
}

const std::list< boost::shared_ptr<decode::Decoder> >&
DecoderStack::stack() const
{
	return _stack;
}

void DecoderStack::push(boost::shared_ptr<decode::Decoder> decoder)
{
	assert(decoder);
	_stack.push_back(decoder);
}

void DecoderStack::remove(int index)
{
	assert(index >= 0);
	assert(index < (int)_stack.size());

	// Find the decoder in the stack
	list< shared_ptr<Decoder> >::iterator iter = _stack.begin();
	for(int i = 0; i < index; i++, iter++)
		assert(iter != _stack.end());

	// Delete the element
	_stack.erase(iter);
}

int64_t DecoderStack::samples_decoded() const
{
	lock_guard<mutex> decode_lock(_output_mutex);
	return _samples_decoded;
}

std::vector<Row> DecoderStack::get_visible_rows() const
{
	lock_guard<mutex> lock(_output_mutex);

	vector<Row> rows;

	BOOST_FOREACH (const shared_ptr<decode::Decoder> &dec, _stack)
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
	lock_guard<mutex> lock(_output_mutex);

	std::map<const Row, decode::RowData>::const_iterator iter =
		_rows.find(row);
	if (iter != _rows.end())
		(*iter).second.get_annotation_subset(dest,
			start_sample, end_sample);
}

QString DecoderStack::error_message()
{
	lock_guard<mutex> lock(_output_mutex);
	return _error_message;
}

void DecoderStack::clear()
{
	_sample_count = 0;
	_frame_complete = false;
	_samples_decoded = 0;
	_error_message = QString();
	_rows.clear();
	_class_rows.clear();
}

void DecoderStack::begin_decode()
{
	shared_ptr<pv::view::LogicSignal> logic_signal;
	shared_ptr<pv::data::Logic> data;

	if (_decode_thread.joinable()) {
		_decode_thread.interrupt();
		_decode_thread.join();
	}

	clear();

	// Add classes
	BOOST_FOREACH (const shared_ptr<decode::Decoder> &dec, _stack)
	{
		assert(dec);
		const srd_decoder *const decc = dec->decoder();
		assert(dec->decoder());

		// Add a row for the decoder if it doesn't have a row list
		if (!decc->annotation_rows)
			_rows[Row(decc)] = decode::RowData();

		// Add the decoder rows
		for (const GSList *l = decc->annotation_rows; l; l = l->next)
		{
			const srd_decoder_annotation_row *const ann_row =
				(srd_decoder_annotation_row *)l->data;
			assert(ann_row);

			const Row row(decc, ann_row);

			// Add a new empty row data object
			_rows[row] = decode::RowData();

			// Map out all the classes
			for (const GSList *ll = ann_row->ann_classes;
				ll; ll = ll->next)
				_class_rows[make_pair(decc,
					GPOINTER_TO_INT(ll->data))] = row;
		}
	}

	// We get the logic data of the first probe in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
	BOOST_FOREACH (const shared_ptr<decode::Decoder> &dec, _stack)
		if (dec && !dec->probes().empty() &&
			((logic_signal = (*dec->probes().begin()).second)) &&
			((data = logic_signal->logic_data())))
			break;

	if (!data)
		return;

	// Check we have a snapshot of data
	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		data->get_snapshots();
	if (snapshots.empty())
		return;
	_snapshot = snapshots.front();

	// Get the samplerate and start time
	_start_time = data->get_start_time();
	_samplerate = data->samplerate();
	if (_samplerate == 0.0)
		_samplerate = 1.0;

	_decode_thread = boost::thread(&DecoderStack::decode_proc, this);
}

uint64_t DecoderStack::get_max_sample_count() const
{
	uint64_t max_sample_count = 0;

	for (map<const Row, RowData>::const_iterator i = _rows.begin();
		i != _rows.end(); i++)
		max_sample_count = max(max_sample_count,
			(*i).second.get_max_sample());

	return max_sample_count;
}

optional<int64_t> DecoderStack::wait_for_data() const
{
	unique_lock<mutex> input_lock(_input_mutex);
	while(!boost::this_thread::interruption_requested() &&
		!_frame_complete && _samples_decoded >= _sample_count)
		_input_cond.wait(input_lock);
	return boost::make_optional(
		!boost::this_thread::interruption_requested() &&
		(_samples_decoded < _sample_count || !_frame_complete),
		_sample_count);
}

void DecoderStack::decode_data(
	const int64_t sample_count, const unsigned int unit_size,
	srd_session *const session)
{
	uint8_t chunk[DecodeChunkLength];

	const unsigned int chunk_sample_count =
		DecodeChunkLength / _snapshot->unit_size();

	for (int64_t i = 0;
		!boost::this_thread::interruption_requested() &&
			i < sample_count;
		i += chunk_sample_count)
	{
		lock_guard<mutex> decode_lock(_global_decode_mutex);

		const int64_t chunk_end = min(
			i + chunk_sample_count, sample_count);
		_snapshot->get_samples(chunk, i, chunk_end);

		if (srd_session_send(session, i, i + sample_count, chunk,
				(chunk_end - i) * unit_size) != SRD_OK) {
			_error_message = tr("Decoder reported an error");
			break;
		}

		{
			lock_guard<mutex> lock(_output_mutex);
			_samples_decoded = chunk_end;
		}
	}

	new_decode_data();
}

void DecoderStack::decode_proc()
{
	optional<int64_t> sample_count;
	srd_session *session;
	srd_decoder_inst *prev_di = NULL;

	assert(data);
	assert(_snapshot);

	// Check that all decoders have the required probes
	BOOST_FOREACH(const shared_ptr<decode::Decoder> &dec, _stack)
		if (!dec->have_required_probes()) {
			_error_message = tr("One or more required probes "
				"have not been specified");
			return;
		}

	// Create the session
	srd_session_new(&session);
	assert(session);

	// Create the decoders
	const unsigned int unit_size = _snapshot->unit_size();

	BOOST_FOREACH(const shared_ptr<decode::Decoder> &dec, _stack)
	{
		srd_decoder_inst *const di = dec->create_decoder_inst(session, unit_size);

		if (!di)
		{
			_error_message = tr("Failed to create decoder instance");
			srd_session_destroy(session);
			return;
		}

		if (prev_di)
			srd_inst_stack (session, prev_di, di);

		prev_di = di;
	}

	// Get the intial sample count
	{
		unique_lock<mutex> input_lock(_input_mutex);
		sample_count = _sample_count = _snapshot->get_sample_count();
	}

	// Start the session
	srd_session_metadata_set(session, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64((uint64_t)_samplerate));

	srd_pd_output_callback_add(session, SRD_OUTPUT_ANN,
		DecoderStack::annotation_callback, this);

	srd_session_start(session);

	do {
		decode_data(*sample_count, unit_size, session);
	} while(_error_message.isEmpty() && (sample_count = wait_for_data()));

	// Destroy the session
	srd_session_destroy(session);
}

void DecoderStack::annotation_callback(srd_proto_data *pdata, void *decoder)
{
	assert(pdata);
	assert(decoder);

	DecoderStack *const d = (DecoderStack*)decoder;
	assert(d);

	lock_guard<mutex> lock(d->_output_mutex);

	const Annotation a(pdata);

	// Find the row
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

	map<const Row, decode::RowData>::iterator row_iter = d->_rows.end();
	
	// Try looking up the sub-row of this class
	const map<pair<const srd_decoder*, int>, Row>::const_iterator r =
		d->_class_rows.find(make_pair(decc, a.format()));
	if (r != d->_class_rows.end())
		row_iter = d->_rows.find((*r).second);
	else
	{
		// Failing that, use the decoder as a key
		row_iter = d->_rows.find(Row(decc));	
	}

	assert(row_iter != d->_rows.end());
	if (row_iter == d->_rows.end()) {
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
		unique_lock<mutex> lock(_input_mutex);
		_sample_count = _snapshot->get_sample_count();
	}
	_input_cond.notify_one();
}

void DecoderStack::on_frame_ended()
{
	{
		unique_lock<mutex> lock(_input_mutex);
		_frame_complete = true;
	}
	_input_cond.notify_one();
}

} // namespace data
} // namespace pv
