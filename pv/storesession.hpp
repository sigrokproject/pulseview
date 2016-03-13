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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_STORESESSION_HPP
#define PULSEVIEW_PV_STORESESSION_HPP

#include <stdint.h>

#include <atomic>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <glibmm/variant.h>

#include <QObject>

namespace sigrok {
class Channel;
class Output;
class OutputFormat;
}

namespace pv {

class Session;

namespace data {
class AnalogSegment;
class LogicSegment;
}

class StoreSession : public QObject
{
	Q_OBJECT

private:
	static const size_t BlockSize;

public:
	StoreSession(const std::string &file_name,
		const std::shared_ptr<sigrok::OutputFormat> &output_format,
		const std::map<std::string, Glib::VariantBase> &options,
		const std::pair<uint64_t, uint64_t> sample_range,
		const Session &session);

	~StoreSession();

	std::pair<int, int> progress() const;

	const QString& error() const;

	bool start();

	void wait();

	void cancel();

private:
	void store_proc(std::vector< std::shared_ptr<sigrok::Channel> > achannel_list,
		std::vector< std::shared_ptr<pv::data::AnalogSegment> > asegment_list,
		std::shared_ptr<pv::data::LogicSegment> lsegment);

Q_SIGNALS:
	void progress_updated();

private:
	const std::string file_name_;
	const std::shared_ptr<sigrok::OutputFormat> output_format_;
	const std::map<std::string, Glib::VariantBase> options_;
	const std::pair<uint64_t, uint64_t> sample_range_;
	const Session &session_;

	std::shared_ptr<sigrok::Output> output_;
	std::ofstream output_stream_;

	std::thread thread_;

	std::atomic<bool> interrupt_;

	std::atomic<int> units_stored_, unit_count_;

	mutable std::mutex mutex_;
	QString error_;

	uint64_t start_sample_, sample_count_;
};

} // pv

#endif // PULSEVIEW_PV_STORESESSION_HPP
