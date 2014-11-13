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

#ifndef PULSEVIEW_PV_SIGSESSION_H
#define PULSEVIEW_PV_SIGSESSION_H

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <QObject>
#include <QString>

struct srd_decoder;
struct srd_channel;

namespace sigrok {
	class Analog;
	class Channel;
	class Device;
	class Logic;
	class Meta;
	class Packet;
	class Session;
}

namespace pv {

class DeviceManager;

namespace data {
class Analog;
class AnalogSnapshot;
class Logic;
class LogicSnapshot;
class SignalData;
}

namespace view {
class DecodeTrace;
class LogicSignal;
class Signal;
}

class SigSession : public QObject
{
	Q_OBJECT

public:
	enum capture_state {
		Stopped,
		AwaitingTrigger,
		Running
	};

public:
	SigSession(DeviceManager &device_manager);

	~SigSession();

	const std::shared_ptr<sigrok::Session>& session() const;

	std::shared_ptr<sigrok::Device> device() const;

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
	void set_device(std::shared_ptr<sigrok::Device> device);

	void set_file(const std::string &name);

	void set_default_device();

	capture_state get_capture_state() const;

	void start_capture(std::function<void (const QString)> error_handler);

	void stop_capture();

	std::set< std::shared_ptr<data::SignalData> > get_data() const;

	std::vector< std::shared_ptr<view::Signal> >
		get_signals() const;

#ifdef ENABLE_DECODE
	bool add_decoder(srd_decoder *const dec);

	std::vector< std::shared_ptr<view::DecodeTrace> >
		get_decode_signals() const;

	void remove_decode_signal(view::DecodeTrace *signal);
#endif

private:
	void set_capture_state(capture_state state);

	void update_signals(std::shared_ptr<sigrok::Device> device);

	std::shared_ptr<view::Signal> signal_from_channel(
		std::shared_ptr<sigrok::Channel> channel) const;

	void read_sample_rate(std::shared_ptr<sigrok::Device>);

private:
	void sample_thread_proc(std::shared_ptr<sigrok::Device> device,
		std::function<void (const QString)> error_handler);

	void feed_in_header(std::shared_ptr<sigrok::Device> device);

	void feed_in_meta(std::shared_ptr<sigrok::Device> device,
		std::shared_ptr<sigrok::Meta> meta);

	void feed_in_frame_begin();

	void feed_in_logic(std::shared_ptr<sigrok::Logic> logic);

	void feed_in_analog(std::shared_ptr<sigrok::Analog> analog);

	void data_feed_in(std::shared_ptr<sigrok::Device> device,
		std::shared_ptr<sigrok::Packet> packet);

private:
	DeviceManager &_device_manager;
	std::shared_ptr<sigrok::Session> _session;

	/**
	 * The device instance that will be used in the next capture session.
	 */
	std::shared_ptr<sigrok::Device> _device;

	std::vector< std::shared_ptr<view::DecodeTrace> > _decode_traces;

	mutable std::mutex _sampling_mutex;
	capture_state _capture_state;

	mutable std::mutex _signals_mutex;
	std::vector< std::shared_ptr<view::Signal> > _signals;

	mutable std::mutex _data_mutex;
	std::shared_ptr<data::Logic> _logic_data;
	std::shared_ptr<data::LogicSnapshot> _cur_logic_snapshot;
	std::map< std::shared_ptr<sigrok::Channel>, std::shared_ptr<data::AnalogSnapshot> >
		_cur_analog_snapshots;

	std::thread _sampling_thread;

Q_SIGNALS:
	void capture_state_changed(int state);

	void signals_changed();

	void frame_began();

	void data_received();

	void frame_ended();
};

} // namespace pv

#endif // PULSEVIEW_PV_SIGSESSION_H
