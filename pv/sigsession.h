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

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <string>
#include <utility>
#include <vector>

#include <QObject>

extern "C" {
#include <libsigrok/libsigrok.h>
}

namespace pv {

namespace data {
class Analog;
class AnalogSnapshot;
class Logic;
class LogicSnapshot;
}

namespace view {
class Signal;
}

class SigSession : public QObject
{
	Q_OBJECT

public:
	enum capture_state {
		Stopped,
		Running
	};

public:
	SigSession();

	~SigSession();

	void load_file(const std::string &name);

	capture_state get_capture_state() const;

	void start_capture(struct sr_dev_inst* sdi, uint64_t record_length,
		uint64_t sample_rate);

	void stop_capture();

	std::vector< boost::shared_ptr<view::Signal> >
		get_signals();

	boost::shared_ptr<data::Logic> get_data();

private:
	void set_capture_state(capture_state state);

private:
	void load_thread_proc(const std::string name);

	void sample_thread_proc(struct sr_dev_inst *sdi,
		uint64_t record_length);

	void feed_in_meta(const sr_dev_inst *sdi,
		const sr_datafeed_meta &meta);

	void feed_in_logic(const sr_datafeed_logic &logic);

	void feed_in_analog(const sr_datafeed_analog &analog);

	void data_feed_in(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);

	static void data_feed_in_proc(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);

private:
	mutable boost::mutex _sampling_mutex;
	capture_state _capture_state;
	uint64_t _sample_rate;

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
