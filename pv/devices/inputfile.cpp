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

#include <cassert>
#include <fstream>

#include <QString>

#include "inputfile.hpp"

namespace pv {
namespace devices {

const std::streamsize InputFile::BufferSize = 16384;

InputFile::InputFile(const std::shared_ptr<sigrok::Context> &context,
	const std::string &file_name,
	std::shared_ptr<sigrok::InputFormat> format,
	const std::map<std::string, Glib::VariantBase> &options) :
	context_(context),
	input_(format->create_input(options)),
	file_name_(file_name),
	interrupt_(false) {
	if (!input_)
		throw QString("Failed to create input");
}

void InputFile::create() {
	session_ = context_->create_session();
}

void InputFile::start() {
}

void InputFile::run() {
	char buffer[BufferSize];
	bool need_device = true;

	assert(session_);
	assert(input_);

	interrupt_ = false;
	std::ifstream f(file_name_);
	while (!interrupt_ && f) {
		f.read(buffer, BufferSize);
		const std::streamsize size = f.gcount();
		if (size == 0)
			break;

		input_->send(buffer, size);

		if (need_device) {
			try {
				device_ = input_->device();
			} catch (sigrok::Error) {
				break;
			}

			session_->add_device(device_);
			need_device = false;
		}

		if (size != BufferSize)
			break;
	}

	input_->end();
}

void InputFile::stop() {
	interrupt_ = true;
}

} // namespace devices
} // namespace pv
