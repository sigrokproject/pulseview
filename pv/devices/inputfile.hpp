/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DEVICE_INPUTFILE_HPP
#define PULSEVIEW_PV_DEVICE_INPUTFILE_HPP

#include <atomic>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "file.hpp"

using std::atomic;
using std::ifstream;
using std::map;
using std::shared_ptr;
using std::streamsize;
using std::string;

namespace pv {
namespace devices {

class InputFile final : public File
{
private:
	static const streamsize BufferSize;

public:
	InputFile(const shared_ptr<sigrok::Context> &context,
		const string &file_name,
		shared_ptr<sigrok::InputFormat> format,
		const map<string, Glib::VariantBase> &options);

	void open();

	void close();

	void start();

	void run();

	void stop();

private:
	const shared_ptr<sigrok::Context> context_;
	const shared_ptr<sigrok::InputFormat> format_;
	const map<string, Glib::VariantBase> options_;
	shared_ptr<sigrok::Input> input_;

	ifstream *f;
	atomic<bool> interrupt_;
};

} // namespace devices
} // namespace pv

#endif // PULSEVIEW_PV_SESSIONS_INPUTFILE_HPP

