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

#ifndef PULSEVIEW_PV_SIGSESSION_H
#define PULSEVIEW_PV_SIGSESSION_H

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>

#include <libsigrok/libsigrok.h>

struct srd_decoder;
struct srd_probe;

namespace pv {

class DeviceManager;

namespace data {
class Analog;
class AnalogSnapshot;
class Logic;
class LogicSnapshot;
}

namespace view {
class DecodeSignal;
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

	struct sr_dev_inst* get_device() const;

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
	void set_device(struct sr_dev_inst *sdi);

	void release_device(struct sr_dev_inst *sdi);

	void load_file(const std::string &name,
		boost::function<void (const QString)> error_handler);

	capture_state get_capture_state() const;

	void start_capture(uint64_t record_length,
		boost::function<void (const QString)> error_handler);

	void stop_capture();

	std::vector< boost::shared_ptr<view::Signal> >
		get_signals() const;

	boost::shared_ptr<data::Logic> get_data();

	void add_decoder(srd_decoder *const dec,
		std::map<const srd_probe*,
			boost::shared_ptr<view::Signal> > probes);

	std::vector< boost::shared_ptr<view::DecodeSignal> >
		get_decode_signals() const;

private:
	void set_capture_state(capture_state state);

	void update_signals(const sr_dev_inst *const sdi);

	bool is_trigger_enabled() const;

	void read_sample_rate(const sr_dev_inst *const sdi);

private:
	/**
	 * Attempts to autodetect the format. Failing that
	 * @param filename The filename of the input file.
	 * @return A pointer to the 'struct sr_input_format' that should be
	 * 	used, or NULL if no input format was selected or
	 * 	auto-detected.
	 */
	static sr_input_format* determine_input_file_format(
		const std::string &filename);

	static sr_input* load_input_file_format(
		const std::string &filename,
		boost::function<void (const QString)> error_handler,
		sr_input_format *format = NULL);

	void load_session_thread_proc(
		boost::function<void (const QString)> error_handler);

	void load_input_thread_proc(const std::string name, sr_input *in,
		boost::function<void (const QString)> error_handler);

	void sample_thread_proc(struct sr_dev_inst *sdi,
		uint64_t record_length,
		boost::function<void (const QString)> error_handler);

	void feed_in_header(const sr_dev_inst *sdi);

	void feed_in_meta(const sr_dev_inst *sdi,
		const sr_datafeed_meta &meta);

	void feed_in_logic(const sr_datafeed_logic &logic);

	void feed_in_analog(const sr_datafeed_analog &analog);

	void data_feed_in(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);

	static void data_feed_in_proc(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet, void *cb_data);

private:
	DeviceManager &_device_manager;

	/**
	 * The device instance that will be used in the next capture session.
	 */
	struct sr_dev_inst *_sdi;

	std::vector< boost::shared_ptr<view::DecodeSignal> > _decode_traces;

	mutable boost::mutex _sampling_mutex;
	capture_state _capture_state;

	mutable boost::mutex _signals_mutex;
	std::vector< boost::shared_ptr<view::Signal> > _signals;

	mutable boost::mutex _data_mutex;
	boost::shared_ptr<data::Logic> _logic_data;
	boost::shared_ptr<data::LogicSnapshot> _cur_logic_snapshot;
	boost::shared_ptr<data::Analog> _analog_data;
	boost::shared_ptr<data::AnalogSnapshot> _cur_analog_snapshot;

	std::auto_ptr<boost::thread> _sampling_thread;

signals:
	void capture_state_changed(int state);

	void signals_changed();

	void data_updated();

private:
	// TODO: This should not be necessary. Multiple concurrent
	// sessions should should be supported and it should be
	// possible to associate a pointer with a sr_session.
	static SigSession *_session;
};

} // namespace pv

#endif // PULSEVIEW_PV_SIGSESSION_H
