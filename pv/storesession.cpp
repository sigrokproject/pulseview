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

#include "storesession.h"

#include <pv/sigsession.h>
#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/view/signal.h>

using std::deque;
using std::dynamic_pointer_cast;
using std::lock_guard;
using std::make_pair;
using std::min;
using std::mutex;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::thread;
using std::vector;

namespace pv {

const size_t StoreSession::BlockSize = 1024 * 1024;

StoreSession::StoreSession(const std::string &file_name,
	const SigSession &session) :
	_file_name(file_name),
	_session(session),
	_interrupt(false),
	_units_stored(0),
	_unit_count(0)
{
}

StoreSession::~StoreSession()
{
	wait();
}

pair<uint64_t, uint64_t> StoreSession::progress() const
{
	return make_pair(_units_stored.load(), _unit_count.load());
}

const QString& StoreSession::error() const
{
	lock_guard<mutex> lock(_mutex);
	return _error;
}

bool StoreSession::start()
{
	set< shared_ptr<data::SignalData> > data_set =
		_session.get_data();
	const vector< shared_ptr<view::Signal> > sigs =
		_session.get_signals();

	// Check we have logic data
	if (data_set.empty() || sigs.empty()) {
		_error = tr("No data to save.");
		return false;
	}

	if (data_set.size() > 1) {
		_error = tr("PulseView currently only has support for "
			"storing a single data stream.");
		return false;
	}

	// Get the logic data
	//shared_ptr<data::SignalData
	shared_ptr<data::Logic> data;
	if (!(data = dynamic_pointer_cast<data::Logic>(*data_set.begin()))) {
		_error = tr("PulseView currently only has support for "
			"storing a logic data.");
		return false;
	}

	// Get the snapshot
	const deque< shared_ptr<data::LogicSnapshot> > &snapshots =
		data->get_snapshots();

	if (snapshots.empty()) {
		_error = tr("No snapshots to save.");
		return false;
	}

	const shared_ptr<data::LogicSnapshot> snapshot(snapshots.front());
	assert(snapshot);

	// Make a list of probes
	char **const probes = new char*[sigs.size() + 1];
	for (size_t i = 0; i < sigs.size(); i++) {
		shared_ptr<view::Signal> sig(sigs[i]);
		assert(sig);
		probes[i] = strdup(sig->get_name().toUtf8().constData());
	}
	probes[sigs.size()] = NULL;

	// Begin storing
	if (sr_session_save_init(_file_name.c_str(),
		data->samplerate(), probes) != SR_OK) {
		_error = tr("Error while saving.");
		return false;
	}

	// Delete the probes array
	for (size_t i = 0; i <= sigs.size(); i++)
		free(probes[i]);
	delete[] probes;

	_thread = std::thread(&StoreSession::store_proc, this, snapshot);
	return true;
}

void StoreSession::wait()
{
	if (_thread.joinable())
		_thread.join();
}

void StoreSession::cancel()
{
	_interrupt = true;
}

void StoreSession::store_proc(shared_ptr<data::LogicSnapshot> snapshot)
{
	assert(snapshot);

	uint64_t start_sample = 0;

	/// TODO: Wrap this in a std::unique_ptr when we transition to C++11
	uint8_t *const data = new uint8_t[BlockSize];
	assert(data);

	const int unit_size = snapshot->unit_size();
	assert(unit_size != 0);

	_unit_count = snapshot->get_sample_count();

	const unsigned int samples_per_block = BlockSize / unit_size;

	while (!_interrupt && start_sample < _unit_count)
	{
		progress_updated();

		const uint64_t end_sample = min(
			start_sample + samples_per_block, _unit_count.load());
		snapshot->get_samples(data, start_sample, end_sample);

		if(sr_session_append(_file_name.c_str(), data, unit_size,
			end_sample - start_sample) != SR_OK)
		{
			_error = tr("Error while saving.");
			break;
		}

		start_sample = end_sample;
		_units_stored = start_sample;
	}

	_unit_count = 0;
	progress_updated();

	delete[] data;
}

} // pv
