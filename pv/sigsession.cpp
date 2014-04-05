/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012-14 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include "sigsession.h"

#include "devicemanager.h"
#include "device/device.h"
#include "device/file.h"

#include "data/analog.h"
#include "data/analogsnapshot.h"
#include "data/decoderstack.h"
#include "data/logic.h"
#include "data/logicsnapshot.h"
#include "data/decode/decoder.h"

#include "view/analogsignal.h"
#include "view/decodetrace.h"
#include "view/logicsignal.h"

#include <assert.h>

#include <stdexcept>

#include <boost/foreach.hpp>

#include <sys/stat.h>

#include <QDebug>

using boost::dynamic_pointer_cast;
using boost::function;
using boost::lock_guard;
using boost::mutex;
using boost::shared_ptr;
using std::list;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace pv {

// TODO: This should not be necessary
SigSession* SigSession::_session = NULL;

SigSession::SigSession(DeviceManager &device_manager) :
	_device_manager(device_manager),
	_capture_state(Stopped)
{
	// TODO: This should not be necessary
	_session = this;

	set_default_device();
}

SigSession::~SigSession()
{
	using pv::device::Device;

	stop_capture();

	if (_sampling_thread.joinable())
		_sampling_thread.join();

	_dev_inst->release();

	// TODO: This should not be necessary
	_session = NULL;
}

shared_ptr<device::DevInst> SigSession::get_device() const
{
	return _dev_inst;
}

void SigSession::set_device(
	shared_ptr<device::DevInst> dev_inst) throw(QString)
{
	using pv::device::Device;

	// Ensure we are not capturing before setting the device
	stop_capture();

	if (_dev_inst) {
		sr_session_datafeed_callback_remove_all();
		_dev_inst->release();
	}

	_dev_inst = dev_inst;
	_decode_traces.clear();

	if (dev_inst) {
		dev_inst->use(this);
		sr_session_datafeed_callback_add(data_feed_in_proc, NULL);
		update_signals(dev_inst);
	}
}

void SigSession::set_file(const string &name) throw(QString)
{
	// Deslect the old device, because file type detection in File::create
	// destorys the old session inside libsigrok.
	set_device(shared_ptr<device::DevInst>());
	set_device(shared_ptr<device::DevInst>(device::File::create(name)));
}

void SigSession::set_default_device()
{
	shared_ptr<pv::device::DevInst> default_device;
	const list< shared_ptr<device::Device> > &devices =
		_device_manager.devices();

	if (!devices.empty()) {
		// Fall back to the first device in the list.
		default_device = devices.front();

		// Try and find the demo device and select that by default
		BOOST_FOREACH (shared_ptr<pv::device::Device> dev, devices)
			if (strcmp(dev->dev_inst()->driver->name,
				"demo") == 0) {
				default_device = dev;
				break;
			}
	}

	set_device(default_device);
}

void SigSession::release_device(device::DevInst *dev_inst)
{
	(void)dev_inst;
	assert(_dev_inst.get() == dev_inst);

	assert(_capture_state == Stopped);
	_dev_inst = shared_ptr<device::DevInst>();
}

SigSession::capture_state SigSession::get_capture_state() const
{
	lock_guard<mutex> lock(_sampling_mutex);
	return _capture_state;
}

void SigSession::start_capture(function<void (const QString)> error_handler)
{
	stop_capture();

	// Check that a device instance has been selected.
	if (!_dev_inst) {
		qDebug() << "No device selected";
		return;
	}

	assert(_dev_inst->dev_inst());

	// Check that at least one probe is enabled
	const GSList *l;
	for (l = _dev_inst->dev_inst()->channels; l; l = l->next) {
		sr_channel *const probe = (sr_channel*)l->data;
		assert(probe);
		if (probe->enabled)
			break;
	}

	if (!l) {
		error_handler(tr("No channels enabled."));
		return;
	}

	// Begin the session
	_sampling_thread = boost::thread(
		&SigSession::sample_thread_proc, this, _dev_inst,
			error_handler);
}

void SigSession::stop_capture()
{
	if (get_capture_state() == Stopped)
		return;

	sr_session_stop();

	// Check that sampling stopped
	if (_sampling_thread.joinable())
		_sampling_thread.join();
}

set< shared_ptr<data::SignalData> > SigSession::get_data() const
{
	lock_guard<mutex> lock(_signals_mutex);
	set< shared_ptr<data::SignalData> > data;
	BOOST_FOREACH(const shared_ptr<view::Signal> sig, _signals) {
		assert(sig);
		data.insert(sig->data());
	}

	return data;
}

vector< shared_ptr<view::Signal> > SigSession::get_signals() const
{
	lock_guard<mutex> lock(_signals_mutex);
	return _signals;
}

#ifdef ENABLE_DECODE
bool SigSession::add_decoder(srd_decoder *const dec)
{
	map<const srd_channel*, shared_ptr<view::LogicSignal> > probes;
	shared_ptr<data::DecoderStack> decoder_stack;

	try
	{
		lock_guard<mutex> lock(_signals_mutex);

		// Create the decoder
		decoder_stack = shared_ptr<data::DecoderStack>(
			new data::DecoderStack(*this, dec));

		// Make a list of all the probes
		std::vector<const srd_channel*> all_probes;
		for(const GSList *i = dec->channels; i; i = i->next)
			all_probes.push_back((const srd_channel*)i->data);
		for(const GSList *i = dec->opt_channels; i; i = i->next)
			all_probes.push_back((const srd_channel*)i->data);

		// Auto select the initial probes
		BOOST_FOREACH(const srd_channel *pdch, all_probes)
			BOOST_FOREACH(shared_ptr<view::Signal> s, _signals)
			{
				shared_ptr<view::LogicSignal> l =
					dynamic_pointer_cast<view::LogicSignal>(s);
				if (l && QString::fromUtf8(pdch->name).
					toLower().contains(
					l->get_name().toLower()))
					probes[pdch] = l;
			}

		assert(decoder_stack);
		assert(!decoder_stack->stack().empty());
		assert(decoder_stack->stack().front());
		decoder_stack->stack().front()->set_probes(probes);

		// Create the decode signal
		shared_ptr<view::DecodeTrace> d(
			new view::DecodeTrace(*this, decoder_stack,
				_decode_traces.size()));
		_decode_traces.push_back(d);
	}
	catch(std::runtime_error e)
	{
		return false;
	}

	signals_changed();

	// Do an initial decode
	decoder_stack->begin_decode();

	return true;
}

vector< shared_ptr<view::DecodeTrace> > SigSession::get_decode_signals() const
{
	lock_guard<mutex> lock(_signals_mutex);
	return _decode_traces;
}

void SigSession::remove_decode_signal(view::DecodeTrace *signal)
{
	for (vector< shared_ptr<view::DecodeTrace> >::iterator i =
		_decode_traces.begin();
		i != _decode_traces.end();
		i++)
		if ((*i).get() == signal)
		{
			_decode_traces.erase(i);
			signals_changed();
			return;
		}
}
#endif

void SigSession::set_capture_state(capture_state state)
{
	lock_guard<mutex> lock(_sampling_mutex);
	const bool changed = _capture_state != state;
	_capture_state = state;
	if(changed)
		capture_state_changed(state);
}

void SigSession::update_signals(shared_ptr<device::DevInst> dev_inst)
{
	assert(dev_inst);
	assert(_capture_state == Stopped);

	unsigned int logic_probe_count = 0;

	// Clear the decode traces
	_decode_traces.clear();

	// Detect what data types we will receive
	if(dev_inst) {
		assert(dev_inst->dev_inst());
		for (const GSList *l = dev_inst->dev_inst()->channels;
			l; l = l->next) {
			const sr_channel *const probe = (const sr_channel *)l->data;
			if (!probe->enabled)
				continue;

			switch(probe->type) {
			case SR_CHANNEL_LOGIC:
				logic_probe_count++;
				break;
			}
		}
	}

	// Create data containers for the logic data snapshots
	{
		lock_guard<mutex> data_lock(_data_mutex);

		_logic_data.reset();
		if (logic_probe_count != 0) {
			_logic_data.reset(new data::Logic(
				logic_probe_count));
			assert(_logic_data);
		}
	}

	// Make the Signals list
	do {
		lock_guard<mutex> lock(_signals_mutex);

		_signals.clear();

		if(!dev_inst)
			break;

		assert(dev_inst->dev_inst());
		for (const GSList *l = dev_inst->dev_inst()->channels;
			l; l = l->next) {
			shared_ptr<view::Signal> signal;
			sr_channel *const probe = (sr_channel *)l->data;
			assert(probe);

			switch(probe->type) {
			case SR_CHANNEL_LOGIC:
				signal = shared_ptr<view::Signal>(
					new view::LogicSignal(dev_inst,
						probe, _logic_data));
				break;

			case SR_CHANNEL_ANALOG:
			{
				shared_ptr<data::Analog> data(
					new data::Analog());
				signal = shared_ptr<view::Signal>(
					new view::AnalogSignal(dev_inst,
						probe, data));
				break;
			}

			default:
				assert(0);
				break;
			}

			assert(signal);
			_signals.push_back(signal);
		}

	} while(0);

	signals_changed();
}

shared_ptr<view::Signal> SigSession::signal_from_probe(
	const sr_channel *probe) const
{
	lock_guard<mutex> lock(_signals_mutex);
	BOOST_FOREACH(shared_ptr<view::Signal> sig, _signals) {
		assert(sig);
		if (sig->probe() == probe)
			return sig;
	}
	return shared_ptr<view::Signal>();
}

void SigSession::read_sample_rate(const sr_dev_inst *const sdi)
{
	GVariant *gvar;
	uint64_t sample_rate = 0;

	// Read out the sample rate
	if(sdi->driver)
	{
		const int ret = sr_config_get(sdi->driver, sdi, NULL,
			SR_CONF_SAMPLERATE, &gvar);
		if (ret != SR_OK) {
			qDebug("Failed to get samplerate\n");
			return;
		}

		sample_rate = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}

	// Set the sample rate of all data
	const set< shared_ptr<data::SignalData> > data_set = get_data();
	BOOST_FOREACH(shared_ptr<data::SignalData> data, data_set) {
		assert(data);
		data->set_samplerate(sample_rate);
	}
}

void SigSession::sample_thread_proc(shared_ptr<device::DevInst> dev_inst,
	function<void (const QString)> error_handler)
{
	assert(dev_inst);
	assert(dev_inst->dev_inst());
	assert(error_handler);

	read_sample_rate(dev_inst->dev_inst());

	try {
		dev_inst->start();
	} catch(const QString e) {
		error_handler(e);
		return;
	}

	set_capture_state(dev_inst->is_trigger_enabled() ?
		AwaitingTrigger : Running);

	dev_inst->run();
	set_capture_state(Stopped);

	// Confirm that SR_DF_END was received
	if (_cur_logic_snapshot)
	{
		qDebug("SR_DF_END was not received.");
		assert(0);
	}
}

void SigSession::feed_in_header(const sr_dev_inst *sdi)
{
	read_sample_rate(sdi);
}

void SigSession::feed_in_meta(const sr_dev_inst *sdi,
	const sr_datafeed_meta &meta)
{
	(void)sdi;

	for (const GSList *l = meta.config; l; l = l->next) {
		const sr_config *const src = (const sr_config*)l->data;
		switch (src->key) {
		case SR_CONF_SAMPLERATE:
			/// @todo handle samplerate changes
			/// samplerate = (uint64_t *)src->value;
			break;
		default:
			// Unknown metadata is not an error.
			break;
		}
	}

	signals_changed();
}

void SigSession::feed_in_frame_begin()
{
	if (_cur_logic_snapshot || !_cur_analog_snapshots.empty())
		frame_began();
}

void SigSession::feed_in_logic(const sr_datafeed_logic &logic)
{
	lock_guard<mutex> lock(_data_mutex);

	if (!_logic_data)
	{
		qDebug() << "Unexpected logic packet";
		return;
	}

	if (!_cur_logic_snapshot)
	{
		// This could be the first packet after a trigger
		set_capture_state(Running);

		// Create a new data snapshot
		_cur_logic_snapshot = shared_ptr<data::LogicSnapshot>(
			new data::LogicSnapshot(logic, _dev_inst->get_sample_limit()));
		_logic_data->push_snapshot(_cur_logic_snapshot);

		// @todo Putting this here means that only listeners querying
		// for logic will be notified. Currently the only user of
		// frame_began is DecoderStack, but in future we need to signal
		// this after both analog and logic sweeps have begun.
		frame_began();
	}
	else
	{
		// Append to the existing data snapshot
		_cur_logic_snapshot->append_payload(logic);
	}

	data_received();
}

void SigSession::feed_in_analog(const sr_datafeed_analog &analog)
{
	lock_guard<mutex> lock(_data_mutex);

	const unsigned int probe_count = g_slist_length(analog.channels);
	const size_t sample_count = analog.num_samples / probe_count;
	const float *data = analog.data;
	bool sweep_beginning = false;

	for (GSList *p = analog.channels; p; p = p->next)
	{
		shared_ptr<data::AnalogSnapshot> snapshot;

		sr_channel *const probe = (sr_channel*)p->data;
		assert(probe);

		// Try to get the snapshot of the probe
		const map< const sr_channel*, shared_ptr<data::AnalogSnapshot> >::
			iterator iter = _cur_analog_snapshots.find(probe);
		if (iter != _cur_analog_snapshots.end())
			snapshot = (*iter).second;
		else
		{
			// If no snapshot was found, this means we havn't
			// created one yet. i.e. this is the first packet
			// in the sweep containing this snapshot.
			sweep_beginning = true;

			// Create a snapshot, keep it in the maps of probes
			snapshot = shared_ptr<data::AnalogSnapshot>(
				new data::AnalogSnapshot(_dev_inst->get_sample_limit()));
			_cur_analog_snapshots[probe] = snapshot;

			// Find the annalog data associated with the probe
			shared_ptr<view::AnalogSignal> sig =
				dynamic_pointer_cast<view::AnalogSignal>(
					signal_from_probe(probe));
			assert(sig);

			shared_ptr<data::Analog> data(sig->analog_data());
			assert(data);

			// Push the snapshot into the analog data.
			data->push_snapshot(snapshot);
		}

		assert(snapshot);

		// Append the samples in the snapshot
		snapshot->append_interleaved_samples(data++, sample_count,
			probe_count);
	}

	if (sweep_beginning) {
		// This could be the first packet after a trigger
		set_capture_state(Running);
	}

	data_received();
}

void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
	const struct sr_datafeed_packet *packet)
{
	assert(sdi);
	assert(packet);

	switch (packet->type) {
	case SR_DF_HEADER:
		feed_in_header(sdi);
		break;

	case SR_DF_META:
		assert(packet->payload);
		feed_in_meta(sdi,
			*(const sr_datafeed_meta*)packet->payload);
		break;

	case SR_DF_FRAME_BEGIN:
		feed_in_frame_begin();
		break;

	case SR_DF_LOGIC:
		assert(packet->payload);
		feed_in_logic(*(const sr_datafeed_logic*)packet->payload);
		break;

	case SR_DF_ANALOG:
		assert(packet->payload);
		feed_in_analog(*(const sr_datafeed_analog*)packet->payload);
		break;

	case SR_DF_END:
	{
		{
			lock_guard<mutex> lock(_data_mutex);
			_cur_logic_snapshot.reset();
			_cur_analog_snapshots.clear();
		}
		frame_ended();
		break;
	}
	}
}

void SigSession::data_feed_in_proc(const struct sr_dev_inst *sdi,
	const struct sr_datafeed_packet *packet, void *cb_data)
{
	(void) cb_data;
	assert(_session);
	_session->data_feed_in(sdi, packet);
}

} // namespace pv
