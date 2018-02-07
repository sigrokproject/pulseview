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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include <QObject>
#include <QSettings>
#include <QString>

#include "util.hpp"
#include "views/viewbase.hpp"

using std::function;
using std::list;
using std::map;
using std::mutex;
using std::recursive_mutex;
using std::shared_ptr;
using std::string;
using std::unordered_set;

struct srd_decoder;
struct srd_channel;

namespace sigrok {
class Analog;
class Channel;
class Device;
class InputFormat;
class Logic;
class Meta;
class Option;
class OutputFormat;
class Packet;
class Session;
}  // namespace sigrok

using sigrok::Option;

namespace pv {

class DeviceManager;

namespace data {
class Analog;
class AnalogSegment;
class DecodeSignal;
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

namespace views {
class ViewBase;
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

	static shared_ptr<sigrok::Context> sr_context;

public:
	Session(DeviceManager &device_manager, QString name);

	~Session();

	DeviceManager& device_manager();

	const DeviceManager& device_manager() const;

	shared_ptr<sigrok::Session> session() const;

	shared_ptr<devices::Device> device() const;

	QString name() const;

	void set_name(QString name);

	const list< shared_ptr<views::ViewBase> > views() const;

	shared_ptr<views::ViewBase> main_view() const;

	shared_ptr<pv::toolbars::MainBar> main_bar() const;

	void set_main_bar(shared_ptr<pv::toolbars::MainBar> main_bar);

	/**
	 * Indicates whether the captured data was saved to disk already or not
	 */
	bool data_saved() const;

	void save_settings(QSettings &settings) const;

	void restore_settings(QSettings &settings);

	/**
	 * Attempts to set device instance, may fall back to demo if needed
	 */
	void select_device(shared_ptr<devices::Device> device);

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
	void set_device(shared_ptr<devices::Device> device);

	void set_default_device();

	void load_init_file(const string &file_name, const string &format);

	void load_file(QString file_name,
		shared_ptr<sigrok::InputFormat> format = nullptr,
		const map<string, Glib::VariantBase> &options =
			map<string, Glib::VariantBase>());

	capture_state get_capture_state() const;

	void start_capture(function<void (const QString)> error_handler);

	void stop_capture();

	double get_samplerate() const;

	uint32_t get_segment_count() const;

	vector<util::Timestamp> get_triggers(uint32_t segment_id) const;

	void register_view(shared_ptr<views::ViewBase> view);

	void deregister_view(shared_ptr<views::ViewBase> view);

	bool has_view(shared_ptr<views::ViewBase> view);

	const unordered_set< shared_ptr<data::SignalBase> > signalbases() const;

	bool all_segments_complete(uint32_t segment_id) const;

#ifdef ENABLE_DECODE
	shared_ptr<data::DecodeSignal> add_decode_signal();

	void remove_decode_signal(shared_ptr<data::DecodeSignal> signal);
#endif

private:
	void set_capture_state(capture_state state);

	void update_signals();

	shared_ptr<data::SignalBase> signalbase_from_channel(
		shared_ptr<sigrok::Channel> channel) const;

	static map<string, Glib::VariantBase> input_format_options(
		vector<string> user_spec,
		map<string, shared_ptr<Option>> fmt_opts);

	void sample_thread_proc(function<void (const QString)> error_handler);

	void free_unused_memory();

	void signal_new_segment();
	void signal_segment_completed();

	void feed_in_header();

	void feed_in_meta(shared_ptr<sigrok::Meta> meta);

	void feed_in_trigger();

	void feed_in_frame_begin();
	void feed_in_frame_end();

	void feed_in_logic(shared_ptr<sigrok::Logic> logic);

	void feed_in_analog(shared_ptr<sigrok::Analog> analog);

	void data_feed_in(shared_ptr<sigrok::Device> device,
		shared_ptr<sigrok::Packet> packet);

Q_SIGNALS:
	void capture_state_changed(int state);
	void device_changed();

	void signals_changed();

	void name_changed();

	void trigger_event(int segment_id, util::Timestamp location);

	void new_segment(int new_segment_id);
	void segment_completed(int segment_id);

	void data_received();

	void add_view(const QString &title, views::ViewType type,
		Session *session);

public Q_SLOTS:
	void on_data_saved();

private:
	DeviceManager &device_manager_;
	shared_ptr<devices::Device> device_;
	QString default_name_, name_;

	list< shared_ptr<views::ViewBase> > views_;
	shared_ptr<pv::views::ViewBase> main_view_;

	shared_ptr<pv::toolbars::MainBar> main_bar_;

	mutable mutex sampling_mutex_; //!< Protects access to capture_state_.
	capture_state capture_state_;

	unordered_set< shared_ptr<data::SignalBase> > signalbases_;
	unordered_set< shared_ptr<data::SignalData> > all_signal_data_;

	/// trigger_list_ contains pairs of <segment_id, timestamp> values.
	vector< std::pair<uint32_t, util::Timestamp> > trigger_list_;

	mutable recursive_mutex data_mutex_;
	shared_ptr<data::Logic> logic_data_;
	uint64_t cur_samplerate_;
	shared_ptr<data::LogicSegment> cur_logic_segment_;
	map< shared_ptr<sigrok::Channel>, shared_ptr<data::AnalogSegment> >
		cur_analog_segments_;
	int32_t highest_segment_id_;

	std::thread sampling_thread_;

	bool out_of_memory_;
	bool data_saved_;
	bool frame_began_;
};

} // namespace pv

#endif // PULSEVIEW_PV_SESSION_HPP
