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
#include <pv/view/logicsignal.h>
#include <pv/view/decode/annotation.h>

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const double DecoderStack::DecodeMargin = 1.0;
const double DecoderStack::DecodeThreshold = 0.2;
const int64_t DecoderStack::DecodeChunkLength = 4096;

mutex DecoderStack::_global_decode_mutex;

DecoderStack::DecoderStack(const srd_decoder *const dec)
{
	_stack.push_back(shared_ptr<decode::Decoder>(
		new decode::Decoder(dec)));
}

DecoderStack::~DecoderStack()
{
	_decode_thread.interrupt();
	_decode_thread.join();
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

const vector< shared_ptr<view::decode::Annotation> >
	DecoderStack::annotations() const
{
	lock_guard<mutex> lock(_mutex);
	return _annotations;
}

QString DecoderStack::error_message()
{
	lock_guard<mutex> lock(_mutex);
	return _error_message;
}

void DecoderStack::begin_decode()
{
	shared_ptr<pv::view::LogicSignal> logic_signal;
	shared_ptr<pv::data::Logic> data;

	_decode_thread.interrupt();
	_decode_thread.join();

	_annotations.clear();

	// We get the logic data of the first probe in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
	BOOST_FOREACH (const shared_ptr<decode::Decoder> &dec, _stack)
		if (dec && !dec->probes().empty() &&
			((logic_signal = (*dec->probes().begin()).second)) &&
			((data = logic_signal->data())))
			break;

	if (!data)
		return;

	// Get the samplerate and start time
	_start_time = data->get_start_time();
	_samplerate = data->get_samplerate();
	if (_samplerate == 0.0)
		_samplerate = 1.0;

	_decode_thread = boost::thread(&DecoderStack::decode_proc, this,
		data);
}

void DecoderStack::clear_snapshots()
{
}

void DecoderStack::decode_proc(shared_ptr<data::Logic> data)
{
	srd_session *session;
	uint8_t chunk[DecodeChunkLength];
	srd_decoder_inst *prev_di = NULL;

	assert(data);

	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		data->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();
	const int64_t sample_count = snapshot->get_sample_count() - 1;

	// Create the session
	srd_session_new(&session);
	assert(session);


	// Create the decoders
	BOOST_FOREACH(const shared_ptr<decode::Decoder> &dec, _stack)
	{
		srd_decoder_inst *const di = dec->create_decoder_inst(session);

		if (!di)
		{
			_error_message = tr("Failed to initialise decoder");
			srd_session_destroy(session);
			return;
		}

		if (prev_di)
			srd_inst_stack (session, prev_di, di);

		prev_di = di;
	}

	// Start the session
	srd_session_metadata_set(session, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64((uint64_t)_samplerate));

	srd_pd_output_callback_add(session, SRD_OUTPUT_ANN,
		DecoderStack::annotation_callback, this);

	srd_session_start(session);

	for (int64_t i = 0;
		!this_thread::interruption_requested() && i < sample_count;
		i += DecodeChunkLength)
	{
		lock_guard<mutex> decode_lock(_global_decode_mutex);

		const int64_t chunk_end = min(
			i + DecodeChunkLength, sample_count);
		snapshot->get_samples(chunk, i, chunk_end);

		if (srd_session_send(session, i, i + sample_count,
				chunk, chunk_end - i) != SRD_OK) {
			_error_message = tr("Failed to initialise decoder");
			break;
		}
	}

	// Destroy the session
	srd_session_destroy(session);
}

void DecoderStack::annotation_callback(srd_proto_data *pdata, void *decoder)
{
	using namespace pv::view::decode;

	assert(pdata);
	assert(decoder);

	DecoderStack *const d = (DecoderStack*)decoder;

	shared_ptr<Annotation> a(new Annotation(pdata));
	lock_guard<mutex> lock(d->_mutex);
	d->_annotations.push_back(a);

	d->new_decode_data();
}

} // namespace data
} // namespace pv
