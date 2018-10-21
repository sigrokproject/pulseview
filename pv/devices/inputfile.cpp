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

#include <cassert>
#include <fstream>
#include <vector>

#include <QDebug>
#include <QString>

#include <pv/globalsettings.hpp>

#include "inputfile.hpp"

using sigrok::InputFormat;

using std::map;
using std::out_of_range;
using std::pair;
using std::shared_ptr;
using std::streamsize;
using std::string;
using std::ifstream;
using std::ios;
using std::vector;

namespace pv {
namespace devices {

// Use a 4MB chunk size for reading a file into memory. Larger values don't
// seem to provide any substancial performance improvements, but can cause
// UI lag and a visually "stuttering" display of the data currently loading.
const streamsize InputFile::BufferSize = (4 * 1024 * 1024);

InputFile::InputFile(const shared_ptr<sigrok::Context> &context,
	const string &file_name,
	shared_ptr<sigrok::InputFormat> format,
	const map<string, Glib::VariantBase> &options) :
	File(file_name),
	context_(context),
	format_(format),
	options_(options),
	interrupt_(false)
{
}

InputFile::InputFile(const shared_ptr<sigrok::Context> &context,
	QSettings &settings):
	File(""),
	context_(context),
	interrupt_(false)
{
	file_name_ = settings.value("filename").toString().toStdString();

	QString format_name = settings.value("format").toString();

	// Find matching format
	const map<string, shared_ptr<InputFormat> > formats = context->input_formats();

	try {
		format_ = formats.at(format_name.toStdString());

		// Restore all saved options
		int options = settings.value("options").toInt();

		for (int i = 0; i < options; i++) {
			settings.beginGroup("option" + QString::number(i));
			QString name = settings.value("name").toString();
			options_[name.toStdString()] = GlobalSettings::restore_variantbase(settings);
			settings.endGroup();
		}

	} catch (out_of_range&) {
		qWarning() << "Could not find input format" << format_name <<
			"needed to restore session input file";
	}
}

void InputFile::save_meta_to_settings(QSettings &settings)
{
	settings.setValue("filename", QString::fromStdString(file_name_));

	settings.setValue("format", QString::fromStdString(format_->name()));

	settings.setValue("options", (int)options_.size());

	int i = 0;
	for (const pair<string, Glib::VariantBase>& option : options_) {
		settings.beginGroup("option" + QString::number(i));
		settings.setValue("name", QString::fromStdString(option.first));
		GlobalSettings::store_variantbase(settings, option.second);
		settings.endGroup();
		i++;
	}
}

void InputFile::open()
{
	if (session_)
		close();
	else
		session_ = context_->create_session();

	if (!format_)
		return;

	input_ = format_->create_input(options_);

	if (!input_)
		throw QString("Failed to create input");

	// open() should add the input device to the session but
	// we can't open the device without sending some data first
	f = new ifstream(file_name_, ios::binary);

	vector<char> buffer(BufferSize);

	f->read(buffer.data(), BufferSize);
	const streamsize size = f->gcount();

	if (size == 0)
		throw QString("Failed to read file");

	input_->send(buffer.data(), size);

	try {
		device_ = input_->device();
	} catch (sigrok::Error& e) {
		throw e;
	}

	session_->add_device(device_);
}

void InputFile::close()
{
	if (session_)
		session_->remove_devices();
}

void InputFile::start()
{
}

void InputFile::run()
{
	if (!input_)
		return;

	if (!f) {
		// Previous call to run() processed the entire file already
		f = new ifstream(file_name_, ios::binary);
		input_->reset();
	}

	vector<char> buffer(BufferSize);

	interrupt_ = false;
	while (!interrupt_ && !f->eof()) {
		f->read(buffer.data(), BufferSize);
		const streamsize size = f->gcount();
		if (size == 0)
			break;

		input_->send(buffer.data(), size);

		if (size != BufferSize)
			break;
	}

	input_->end();

	delete f;
	f = nullptr;
}

void InputFile::stop()
{
	interrupt_ = true;
}

} // namespace devices
} // namespace pv
