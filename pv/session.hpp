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

#ifndef PULSEVIEW_PV_SESSION_HPP
#define PULSEVIEW_PV_SESSION_HPP

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
// Windows: Avoid boost/thread namespace pollution (which includes windows.h).
#define NOGDI
#define NORESOURCE
#endif
#include <boost/thread/shared_mutex.hpp>

#include <QObject>
#include <QSettings>
#include <QString>

#include "util.hpp"

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
class AnalogSegment;
class Logic;
class LogicSegment;
class SignalBase;
class SignalData;
}

namespace devices {
class Device;
}

namespace toolbars {
class MainBar;
}

namespace view {
class View;
}

class Session : public QObject
{
	Q_OBJECT

public:
	enum capture_state {
		Stopped,
		AwaitingTrigger,
		Running
	};

public:
	Session(DeviceManager &device_manager, QString name);

	~Session();

	DeviceManager& device_manager();

	const DeviceManager& device_manager() const;

	std::shared_ptr<sigrok::Session> session() const;

	std::shared_ptr<devices::Device> device() const;

	QString name() const;

	void set_name(QString name);

	std::shared_ptr<pv::view::View> main_view() const;

	void set_main_bar(std::shared_ptr<pv::toolbars::MainBar> main_bar);

	std::shared_ptr<pv::toolbars::MainBar> main_bar() const;

	void save_settings(QSettings &settings) const;

	void restore_settings(QSettings &settings);

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
	void set_device(std::shared_ptr<devices::Device> device);

	void set_default_device();

	capture_state get_capture_state() const;

	void start_capture(std::function<void (const QString)> error_handler);

	void stop_capture();

	double get_samplerate() const;

	void register_view(std::shared_ptr<pv::view::View> view);

	void deregister_view(std::shared_ptr<pv::view::View> view);

	bool has_view(std::shared_ptr<pv::view::View> view);

	const std::unordered_set< std::shared_ptr<data::SignalBase> >
		signalbases() const;

#ifdef ENABLE_DECODE
	bool add_decoder(srd_decoder *const dec);

	void remove_decode_signal(std::shared_ptr<data::SignalBase> signalbase);
#endif

private:
	void set_capture_state(capture_state state);

	void update_signals();

	std::shared_ptr<data::SignalBase> signalbase_from_channel(
		std::shared_ptr<sigrok::Channel> channel) const;

private:
	void sample_thread_proc(std::function<void (const QString)> error_handler);

	void feed_in_header();

	void feed_in_meta(std::shared_ptr<sigrok::Meta> meta);

	void feed_in_trigger();

	void feed_in_frame_begin();

	void feed_in_logic(std::shared_ptr<sigrok::Logic> logic);

	void feed_in_analog(std::shared_ptr<sigrok::Analog> analog);

	void data_feed_in(std::shared_ptr<sigrok::Device> device,
		std::shared_ptr<sigrok::Packet> packet);

private:
	DeviceManager &device_manager_;
	std::shared_ptr<devices::Device> device_;
	QString default_name_, name_;

	std::unordered_set< std::shared_ptr<pv::view::View> > views_;
	std::shared_ptr<pv::view::View> main_view_;

	std::shared_ptr<pv::toolbars::MainBar> main_bar_;

	mutable std::mutex sampling_mutex_; //!< Protects access to capture_state_.
	capture_state capture_state_;

	std::unordered_set< std::shared_ptr<data::SignalBase> > signalbases_;
	std::unordered_set< std::shared_ptr<data::SignalData> > all_signal_data_;

	mutable std::recursive_mutex data_mutex_;
	std::shared_ptr<data::Logic> logic_data_;
	uint64_t cur_samplerate_;
	std::shared_ptr<data::LogicSegment> cur_logic_segment_;
	std::map< std::shared_ptr<sigrok::Channel>, std::shared_ptr<data::AnalogSegment> >
		cur_analog_segments_;

	std::thread sampling_thread_;

	bool out_of_memory_;

Q_SIGNALS:
	void capture_state_changed(int state);
	void device_changed();

	void signals_changed();

	void name_changed();

	void trigger_event(util::Timestamp location);

	void frame_began();

	void data_received();

	void frame_ended();
};

} // namespace pv

#endif // PULSEVIEW_PV_SESSION_HPP
