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

#include "data/analog.h"
#include "data/analogsnapshot.h"
#include "data/decoderstack.h"
#include "data/logic.h"
#include "data/logicsnapshot.h"
#include "data/decode/decoder.h"

#include "view/analogsignal.h"
#include "view/decodetrace.h"
#include "view/logicsignal.h"

#include <cassert>
#include <mutex>
#include <stdexcept>

#include <sys/stat.h>

#include <QDebug>

#include <libsigrok/libsigrok.hpp>

using std::dynamic_pointer_cast;
using std::function;
using std::lock_guard;
using std::mutex;
using std::list;
using std::map;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

using sigrok::Analog;
using sigrok::Channel;
using sigrok::ChannelType;
using sigrok::ConfigKey;
using sigrok::DatafeedCallbackFunction;
using sigrok::Device;
using sigrok::Error;
using sigrok::HardwareDevice;
using sigrok::Header;
using sigrok::Logic;
using sigrok::Meta;
using sigrok::Packet;
using sigrok::PacketPayload;
using sigrok::Session;
using sigrok::SessionDevice;

using Glib::VariantBase;
using Glib::Variant;

namespace pv {

// TODO: This should not be necessary
shared_ptr<Session> SigSession::_sr_session = nullptr;

SigSession::SigSession(DeviceManager &device_manager) :
	_device_manager(device_manager),
	_capture_state(Stopped)
{
	// TODO: This should not be necessary
	_sr_session = device_manager.context()->create_session();

	set_default_device();
}

SigSession::~SigSession()
{
	// Stop and join to the thread
	stop_capture();
}

shared_ptr<Device> SigSession::get_device() const
{
	return _device;
}

void SigSession::set_device(shared_ptr<Device> device)
{
	// Ensure we are not capturing before setting the device
	stop_capture();

	// Are we setting a session device?
	auto session_device = dynamic_pointer_cast<SessionDevice>(device);
	// Did we have a session device selected previously?
	auto prev_session_device = dynamic_pointer_cast<SessionDevice>(_device);

	if (_device) {
		_sr_session->remove_datafeed_callbacks();
		if (!prev_session_device) {
			_device->close();
			_sr_session->remove_devices();
		}
	}

	if (session_device)
		_sr_session = session_device->parent();

	_device = device;
	_decode_traces.clear();

	if (device) {
		if (!session_device)
		{
			_sr_session = _device_manager.context()->create_session();
			device->open();
			_sr_session->add_device(device);
		}
		_sr_session->add_datafeed_callback([=]
			(shared_ptr<Device> device, shared_ptr<Packet> packet) {
				data_feed_in(device, packet);
			});
		update_signals(device);
	}
}

void SigSession::set_file(const string &name)
{
	_sr_session = _device_manager.context()->load_session(name);
	_device = _sr_session->devices()[0];
	_decode_traces.clear();
	_sr_session->add_datafeed_callback([=]
		(shared_ptr<Device> device, shared_ptr<Packet> packet) {
			data_feed_in(device, packet);
		});
	_device_manager.update_display_name(_device);
	update_signals(_device);
}

void SigSession::set_default_device()
{
	shared_ptr<HardwareDevice> default_device;
	const list< shared_ptr<HardwareDevice> > &devices =
		_device_manager.devices();

	if (!devices.empty()) {
		// Fall back to the first device in the list.
		default_device = devices.front();

		// Try and find the demo device and select that by default
		for (shared_ptr<HardwareDevice> dev : devices)
			if (dev->driver()->name().compare("demo") == 0) {
				default_device = dev;
				break;
			}

		set_device(default_device);
	}
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
	if (!_device) {
		qDebug() << "No device selected";
		return;
	}

	// Check that at least one channel is enabled
	auto channels = _device->channels();
	bool enabled = std::any_of(channels.begin(), channels.end(),
		[](shared_ptr<Channel> channel) { return channel->enabled(); });

	if (!enabled) {
		error_handler(tr("No channels enabled."));
		return;
	}

	// Begin the session
	_sampling_thread = std::thread(
		&SigSession::sample_thread_proc, this, _device,
			error_handler);
}

void SigSession::stop_capture()
{
	if (get_capture_state() != Stopped)
		_sr_session->stop();

	// Check that sampling stopped
	if (_sampling_thread.joinable())
		_sampling_thread.join();
}

set< shared_ptr<data::SignalData> > SigSession::get_data() const
{
	lock_guard<mutex> lock(_signals_mutex);
	set< shared_ptr<data::SignalData> > data;
	for (const shared_ptr<view::Signal> sig : _signals) {
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
	map<const srd_channel*, shared_ptr<view::LogicSignal> > channels;
	shared_ptr<data::DecoderStack> decoder_stack;

	try
	{
		lock_guard<mutex> lock(_signals_mutex);

		// Create the decoder
		decoder_stack = shared_ptr<data::DecoderStack>(
			new data::DecoderStack(*this, dec));

		// Make a list of all the channels
		std::vector<const srd_channel*> all_channels;
		for(const GSList *i = dec->channels; i; i = i->next)
			all_channels.push_back((const srd_channel*)i->data);
		for(const GSList *i = dec->opt_channels; i; i = i->next)
			all_channels.push_back((const srd_channel*)i->data);

		// Auto select the initial channels
		for (const srd_channel *pdch : all_channels)
			for (shared_ptr<view::Signal> s : _signals)
			{
				shared_ptr<view::LogicSignal> l =
					dynamic_pointer_cast<view::LogicSignal>(s);
				if (l && QString::fromUtf8(pdch->name).
					toLower().contains(
					l->get_name().toLower()))
					channels[pdch] = l;
			}

		assert(decoder_stack);
		assert(!decoder_stack->stack().empty());
		assert(decoder_stack->stack().front());
		decoder_stack->stack().front()->set_channels(channels);

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
	for (auto i = _decode_traces.begin(); i != _decode_traces.end(); i++)
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

void SigSession::update_signals(shared_ptr<Device> device)
{
	assert(device);
	assert(_capture_state == Stopped);

	// Clear the decode traces
	_decode_traces.clear();

	// Detect what data types we will receive
	auto channels = device->channels();
	unsigned int logic_channel_count = std::count_if(
		channels.begin(), channels.end(),
		[] (shared_ptr<Channel> channel) {
			return channel->type() == ChannelType::LOGIC; });

	// Create data containers for the logic data snapshots
	{
		lock_guard<mutex> data_lock(_data_mutex);

		_logic_data.reset();
		if (logic_channel_count != 0) {
			_logic_data.reset(new data::Logic(
				logic_channel_count));
			assert(_logic_data);
		}
	}

	// Make the Signals list
	{
		lock_guard<mutex> lock(_signals_mutex);

		_signals.clear();

		for (auto channel : device->channels()) {
			shared_ptr<view::Signal> signal;

			switch(channel->type()->id()) {
			case SR_CHANNEL_LOGIC:
				signal = shared_ptr<view::Signal>(
					new view::LogicSignal(*this, device,
						channel, _logic_data));
				break;

			case SR_CHANNEL_ANALOG:
			{
				shared_ptr<data::Analog> data(
					new data::Analog());
				signal = shared_ptr<view::Signal>(
					new view::AnalogSignal(
						*this, channel, data));
				break;
			}

			default:
				assert(0);
				break;
			}

			assert(signal);
			_signals.push_back(signal);
		}

	}

	signals_changed();
}

shared_ptr<view::Signal> SigSession::signal_from_channel(
	shared_ptr<Channel> channel) const
{
	lock_guard<mutex> lock(_signals_mutex);
	for (shared_ptr<view::Signal> sig : _signals) {
		assert(sig);
		if (sig->channel() == channel)
			return sig;
	}
	return shared_ptr<view::Signal>();
}

void SigSession::read_sample_rate(shared_ptr<Device> device)
{
	uint64_t sample_rate = VariantBase::cast_dynamic<Variant<guint64>>(
		device->config_get(ConfigKey::SAMPLERATE)).get();

	// Set the sample rate of all data
	const set< shared_ptr<data::SignalData> > data_set = get_data();
	for (shared_ptr<data::SignalData> data : data_set) {
		assert(data);
		data->set_samplerate(sample_rate);
	}
}

void SigSession::sample_thread_proc(shared_ptr<Device> device,
	function<void (const QString)> error_handler)
{
	assert(device);
	assert(error_handler);

	read_sample_rate(device);

	try {
		_sr_session->start();
	} catch(Error e) {
		error_handler(e.what());
		return;
	}

	set_capture_state(_sr_session->trigger() ?
		AwaitingTrigger : Running);

	_sr_session->run();
	set_capture_state(Stopped);

	// Confirm that SR_DF_END was received
	if (_cur_logic_snapshot)
	{
		qDebug("SR_DF_END was not received.");
		assert(0);
	}
}

void SigSession::feed_in_header(shared_ptr<Device> device)
{
	read_sample_rate(device);
}

void SigSession::feed_in_meta(shared_ptr<Device> device,
	shared_ptr<Meta> meta)
{
	(void)device;

	for (auto entry : meta->config()) {
		switch (entry.first->id()) {
		case SR_CONF_SAMPLERATE:
			/// @todo handle samplerate changes
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

void SigSession::feed_in_logic(shared_ptr<Logic> logic)
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

		// Get sample limit.
		uint64_t sample_limit;
		try {
			sample_limit = VariantBase::cast_dynamic<Variant<guint64>>(
				_device->config_get(ConfigKey::LIMIT_SAMPLES)).get();
		} catch (Error) {
			sample_limit = 0;
		}

		// Create a new data snapshot
		_cur_logic_snapshot = shared_ptr<data::LogicSnapshot>(
			new data::LogicSnapshot(logic, sample_limit));
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

void SigSession::feed_in_analog(shared_ptr<Analog> analog)
{
	lock_guard<mutex> lock(_data_mutex);

	const vector<shared_ptr<Channel>> channels = analog->channels();
	const unsigned int channel_count = channels.size();
	const size_t sample_count = analog->num_samples() / channel_count;
	const float *data = analog->data_pointer();
	bool sweep_beginning = false;

	for (auto channel : channels)
	{
		shared_ptr<data::AnalogSnapshot> snapshot;

		// Try to get the snapshot of the channel
		const map< shared_ptr<Channel>, shared_ptr<data::AnalogSnapshot> >::
			iterator iter = _cur_analog_snapshots.find(channel);
		if (iter != _cur_analog_snapshots.end())
			snapshot = (*iter).second;
		else
		{
			// If no snapshot was found, this means we havn't
			// created one yet. i.e. this is the first packet
			// in the sweep containing this snapshot.
			sweep_beginning = true;

			// Get sample limit.
			uint64_t sample_limit;
			try {
				sample_limit = VariantBase::cast_dynamic<Variant<guint64>>(
					_device->config_get(ConfigKey::LIMIT_SAMPLES)).get();
			} catch (Error) {
				sample_limit = 0;
			}

			// Create a snapshot, keep it in the maps of channels
			snapshot = shared_ptr<data::AnalogSnapshot>(
				new data::AnalogSnapshot(sample_limit));
			_cur_analog_snapshots[channel] = snapshot;

			// Find the annalog data associated with the channel
			shared_ptr<view::AnalogSignal> sig =
				dynamic_pointer_cast<view::AnalogSignal>(
					signal_from_channel(channel));
			assert(sig);

			shared_ptr<data::Analog> data(sig->analog_data());
			assert(data);

			// Push the snapshot into the analog data.
			data->push_snapshot(snapshot);
		}

		assert(snapshot);

		// Append the samples in the snapshot
		snapshot->append_interleaved_samples(data++, sample_count,
			channel_count);
	}

	if (sweep_beginning) {
		// This could be the first packet after a trigger
		set_capture_state(Running);
	}

	data_received();
}

void SigSession::data_feed_in(shared_ptr<Device> device, shared_ptr<Packet> packet)
{
	assert(device);
	assert(packet);

	switch (packet->type()->id()) {
	case SR_DF_HEADER:
		feed_in_header(device);
		break;

	case SR_DF_META:
		feed_in_meta(device, dynamic_pointer_cast<Meta>(packet->payload()));
		break;

	case SR_DF_FRAME_BEGIN:
		feed_in_frame_begin();
		break;

	case SR_DF_LOGIC:
		feed_in_logic(dynamic_pointer_cast<Logic>(packet->payload()));
		break;

	case SR_DF_ANALOG:
		feed_in_analog(dynamic_pointer_cast<Analog>(packet->payload()));
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
	default:
		break;
	}
}

} // namespace pv
