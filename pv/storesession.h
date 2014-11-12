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

#ifndef PULSEVIEW_PV_STORESESSION_H
#define PULSEVIEW_PV_STORESESSION_H

#include <stdint.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <QObject>

namespace sigrok {
class Output;
}

namespace pv {

class SigSession;

namespace data {
class LogicSnapshot;
}

class StoreSession : public QObject
{
	Q_OBJECT

private:
	static const size_t BlockSize;

public:
	StoreSession(const std::string &file_name,
		const SigSession &session);

	~StoreSession();

	std::pair<int, int> progress() const;

	const QString& error() const;

	bool start();

	void wait();

	void cancel();

private:
	void store_proc(std::shared_ptr<pv::data::LogicSnapshot> snapshot);

Q_SIGNALS:
	void progress_updated();

private:
	const std::string _file_name;
	const SigSession &_session;

	std::shared_ptr<sigrok::Output> _output;

	std::thread _thread;

	std::atomic<bool> _interrupt;

	std::atomic<int> _units_stored, _unit_count;

	mutable std::mutex _mutex;
	QString _error;
};

} // pv

#endif // PULSEVIEW_PV_STORESESSION_H
