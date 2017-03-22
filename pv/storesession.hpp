/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_STORESESSION_HPP
#define PULSEVIEW_PV_STORESESSION_HPP

#include <cstdint>
#include <atomic>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <glibmm/variant.h>

#include <QObject>

using std::atomic;
using std::string;
using std::shared_ptr;
using std::pair;
using std::map;
using std::vector;
using std::thread;
using std::mutex;
using std::ofstream;

namespace sigrok {
class Output;
class OutputFormat;
}

namespace pv {

class Session;

namespace data {
class SignalBase;
class AnalogSegment;
class LogicSegment;
}

class StoreSession : public QObject
{
	Q_OBJECT

private:
	static const size_t BlockSize;

public:
	StoreSession(const string &file_name,
		const shared_ptr<sigrok::OutputFormat> &output_format,
		const map<string, Glib::VariantBase> &options,
		const pair<uint64_t, uint64_t> sample_range,
		const Session &session);

	~StoreSession();

	pair<int, int> progress() const;

	const QString& error() const;

	bool start();

	void wait();

	void cancel();

private:
	void store_proc(vector< shared_ptr<data::SignalBase> > achannel_list,
		vector< shared_ptr<pv::data::AnalogSegment> > asegment_list,
		shared_ptr<pv::data::LogicSegment> lsegment);

Q_SIGNALS:
	void progress_updated();

	void store_successful();

private:
	const string file_name_;
	const shared_ptr<sigrok::OutputFormat> output_format_;
	const map<string, Glib::VariantBase> options_;
	const pair<uint64_t, uint64_t> sample_range_;
	const Session &session_;

	shared_ptr<sigrok::Output> output_;
	ofstream output_stream_;

	std::thread thread_;

	atomic<bool> interrupt_;

	atomic<int> units_stored_, unit_count_;

	mutable mutex mutex_;
	QString error_;

	uint64_t start_sample_, sample_count_;
};

}  // namespace pv

#endif // PULSEVIEW_PV_STORESESSION_HPP
