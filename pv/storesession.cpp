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

#ifdef _WIN32
// Windows: Avoid boost/thread namespace pollution (which includes windows.h).
#define NOGDI
#define NORESOURCE
#endif
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

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
using sigrok::OutputFlag;

namespace pv {

const size_t StoreSession::BlockSize = 1024 * 1024;

StoreSession::StoreSession(const std::string &file_name,
	const shared_ptr<OutputFormat> &output_format,
	const map<string, VariantBase> &options,
	const std::pair<uint64_t, uint64_t> sample_range,
	const Session &session) :
	file_name_(file_name),
	output_format_(output_format),
	options_(options),
	sample_range_(sample_range),
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
	const unordered_set< shared_ptr<view::Signal> > sigs(session_.signals());

	// Add enabled channels to the data set
	set< shared_ptr<data::SignalData> > data_set;

	for (shared_ptr<view::Signal> signal : sigs)
		if (signal->enabled())
			data_set.insert(signal->data());

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
			"storing logic data.");
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

	// Check whether the user wants to export a certain sample range
	if (sample_range_.first == sample_range_.second) {
		start_sample_ = 0;
		sample_count_ = segment->get_sample_count();
	} else {
		start_sample_ = std::min(sample_range_.first, sample_range_.second);
		sample_count_ = std::abs(sample_range_.second - sample_range_.first);
	}

	// Begin storing
	try {
		const auto context = session_.device_manager().context();
		auto device = session_.device()->device();

		map<string, Glib::VariantBase> options = options_;

		if (!output_format_->test_flag(OutputFlag::INTERNAL_IO_HANDLING))
			output_stream_.open(file_name_, ios_base::binary |
					ios_base::trunc | ios_base::out);

		output_ = output_format_->create_output(file_name_, device, options);
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

	unsigned progress_scale = 0;

	/// TODO: Wrap this in a std::unique_ptr when we transition to C++11
	uint8_t *const data = new uint8_t[BlockSize];
	assert(data);

	const int unit_size = segment->unit_size();
	assert(unit_size != 0);

	// Qt needs the progress values to fit inside an int.  If they would
	// not, scale the current and max values down until they do.
	while ((sample_count_ >> progress_scale) > INT_MAX)
		progress_scale ++;

	unit_count_ = sample_count_ >> progress_scale;

	const unsigned int samples_per_block = BlockSize / unit_size;

	while (!interrupt_ && sample_count_)
	{
		progress_updated();

		const uint64_t packet_len =
			std::min((uint64_t)samples_per_block, sample_count_);

		segment->get_samples(data, start_sample_, start_sample_ + packet_len);

		size_t length = packet_len * unit_size;

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

		sample_count_ -= packet_len;
		start_sample_ += packet_len;
		units_stored_ = unit_count_ - (sample_count_ >> progress_scale);
	}

	// Zeroing the progress variables indicates completion
	units_stored_ = unit_count_ = 0;

	progress_updated();

	output_.reset();
	output_stream_.close();

	delete[] data;
}

} // pv
