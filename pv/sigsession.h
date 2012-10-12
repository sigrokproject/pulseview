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

#ifndef SIGSESSION_H
#define SIGSESSION_H

#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

#include <QObject>

extern "C" {
#include <libsigrok/libsigrok.h>
}

namespace pv {

class LogicData;
class LogicDataSnapshot;
class Signal;

class SigSession : public QObject
{
	Q_OBJECT

public:
	SigSession();

	~SigSession();

	void load_file(const std::string &name);

	void start_capture(struct sr_dev_inst* sdi, uint64_t record_length,
		uint64_t sample_rate);

	std::vector< boost::shared_ptr<Signal> >&
		get_signals();

	boost::shared_ptr<LogicData> get_data();

private:
	void data_feed_in(const struct sr_dev_inst *sdi,
		struct sr_datafeed_packet *packet);

	static void data_feed_in_proc(const struct sr_dev_inst *sdi,
		struct sr_datafeed_packet *packet);

private:
	std::vector< boost::shared_ptr<Signal> > _signals;
	boost::shared_ptr<LogicData> _logic_data;
	boost::shared_ptr<LogicDataSnapshot> _cur_logic_snapshot;

signals:
	void data_updated();

private:
	// TODO: This should not be necessary. Multiple concurrent
	// sessions should should be supported and it should be
	// possible to associate a pointer with a sr_session.
	static SigSession *_session;
};

} // namespace pv

#endif // SIGSESSION_H
