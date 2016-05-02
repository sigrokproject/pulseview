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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_DEVICE_INPUTFILE_HPP
#define PULSEVIEW_PV_DEVICE_INPUTFILE_HPP

#include <atomic>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "file.hpp"

namespace pv {
namespace devices {

class InputFile final : public File
{
private:
	static const std::streamsize BufferSize;

public:
	InputFile(const std::shared_ptr<sigrok::Context> &context,
		const std::string &file_name,
		std::shared_ptr<sigrok::InputFormat> format,
		const std::map<std::string, Glib::VariantBase> &options);

	void open();

	void close();

	void start();

	void run();

	void stop();

private:
	const std::shared_ptr<sigrok::Context> context_;
	const std::shared_ptr<sigrok::InputFormat> format_;
	const std::map<std::string, Glib::VariantBase> options_;
	std::shared_ptr<sigrok::Input> input_;

	std::ifstream *f;
	std::atomic<bool> interrupt_;
};

} // namespace devices
} // namespace pv

#endif // PULSEVIEW_PV_SESSIONS_INPUTFILE_HPP

