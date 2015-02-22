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

#include <cassert>

#include "storesession.hpp"

#include <pv/devicemanager.hpp>
#include <pv/session.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <pv/devices/device.hpp>
#include <pv/view/signal.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

using boost::shared_lock;
using boost::shared_mutex;

using std::deque;
using std::dynamic_pointer_cast;
using std::ios_base;
using std::lock_guard;
using std::make_pair;
using std::map;
using std::min;
using std::mutex;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unordered_set;
using std::vector;

using Glib::VariantBase;

using sigrok::ConfigKey;
using sigrok::Error;
using sigrok::OutputFormat;

namespace pv {

const size_t StoreSession::BlockSize = 1024 * 1024;

StoreSession::StoreSession(const std::string &file_name,
	const shared_ptr<OutputFormat> &output_format,
	const map<string, VariantBase> &options, const Session &session) :
	file_name_(file_name),
	output_format_(output_format),
	options_(options),
	session_(session),
	interrupt_(false),
	units_stored_(0),
	unit_count_(0)
{
}

StoreSession::~StoreSession()
{
	wait();
}

pair<int, int> StoreSession::progress() const
{
	return make_pair(units_stored_.load(), unit_count_.load());
}

const QString& StoreSession::error() const
{
	lock_guard<mutex> lock(mutex_);
	return error_;
}

bool StoreSession::start()
{
	set< shared_ptr<data::SignalData> > data_set =
		session_.get_data();

	shared_lock<shared_mutex> lock(session_.signals_mutex());
	const unordered_set< shared_ptr<view::Signal> > &sigs(
		session_.signals());

	// Check we have logic data
	if (data_set.empty() || sigs.empty()) {
		error_ = tr("No data to save.");
		return false;
	}

	if (data_set.size() > 1) {
		error_ = tr("PulseView currently only has support for "
			"storing a single data stream.");
		return false;
	}

	// Get the logic data
	shared_ptr<data::Logic> data;
	if (!(data = dynamic_pointer_cast<data::Logic>(*data_set.begin()))) {
		error_ = tr("PulseView currently only has support for "
			"storing a logic data.");
		return false;
	}

	// Get the segment
	const deque< shared_ptr<data::LogicSegment> > &segments =
		data->logic_segments();

	if (segments.empty()) {
		error_ = tr("No segments to save.");
		return false;
	}

	const shared_ptr<data::LogicSegment> segment(segments.front());
	assert(segment);

	// Begin storing
	try {
		const auto context = session_.device_manager().context();
		auto device = session_.device()->device();

		map<string, Glib::VariantBase> options = options_;

		// If the output has the capability to write files, use it.
		// Otherwise, open the output stream.
		const auto opt_list = output_format_->options();
		if (opt_list.find("filename") != opt_list.end())
			options["filename"] =
				Glib::Variant<Glib::ustring>::create(file_name_);
		else
			output_stream_.open(file_name_, ios_base::binary |
				ios_base::trunc | ios_base::out);

		output_ = output_format_->create_output(device, options);
		auto meta = context->create_meta_packet(
			{{ConfigKey::SAMPLERATE, Glib::Variant<guint64>::create(
				segment->samplerate())}});
		output_->receive(meta);
	} catch (Error error) {
		error_ = tr("Error while saving.");
		return false;
	}

	thread_ = std::thread(&StoreSession::store_proc, this, segment);
	return true;
}

void StoreSession::wait()
{
	if (thread_.joinable())
		thread_.join();
}

void StoreSession::cancel()
{
	interrupt_ = true;
}

void StoreSession::store_proc(shared_ptr<data::LogicSegment> segment)
{
	assert(segment);

	uint64_t start_sample = 0, sample_count;
	unsigned progress_scale = 0;

	/// TODO: Wrap this in a std::unique_ptr when we transition to C++11
	uint8_t *const data = new uint8_t[BlockSize];
	assert(data);

	const int unit_size = segment->unit_size();
	assert(unit_size != 0);

	sample_count = segment->get_sample_count();

	// Qt needs the progress values to fit inside an int.  If they would
	// not, scale the current and max values down until they do.
	while ((sample_count >> progress_scale) > INT_MAX)
		progress_scale ++;

	unit_count_ = sample_count >> progress_scale;

	const unsigned int samples_per_block = BlockSize / unit_size;

	while (!interrupt_ && start_sample < sample_count)
	{
		progress_updated();

		const uint64_t end_sample = min(
			start_sample + samples_per_block, sample_count);
		segment->get_samples(data, start_sample, end_sample);

		size_t length = end_sample - start_sample;

		try {
			const auto context = session_.device_manager().context();
			auto logic = context->create_logic_packet(data, length, unit_size);
			const string data = output_->receive(logic);
			if (output_stream_.is_open())
				output_stream_ << data;
		} catch (Error error) {
			error_ = tr("Error while saving.");
			break;
		}

		start_sample = end_sample;
		units_stored_ = start_sample >> progress_scale;
	}

	// Zeroing the progress variables indicates completion
	units_stored_ = unit_count_ = 0;

	progress_updated();

	output_.reset();
	output_stream_.close();

	delete[] data;
}

} // pv
