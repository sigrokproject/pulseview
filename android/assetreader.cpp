/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Daniel Elstner <daniel.kitta@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "assetreader.hpp"

#include <memory>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>

#include <libsigrok/libsigrok.h>

using namespace pv;

using std::string;
using std::unique_ptr;

void AndroidAssetReader::open(struct sr_resource *res, string name)
{
	if (res->type == SR_RESOURCE_FIRMWARE) {
		auto path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
						   QString::fromStdString("sigrok-firmware/" + name));
		if (path.isEmpty())
			path = QString::fromStdString("assets:/sigrok-firmware/" + name);

		unique_ptr<QFile> file {new QFile{path}};

		if (!file->open(QIODevice::ReadOnly))
			throw sigrok::Error{SR_ERR};

		const auto size = file->size();
		if (size < 0)
			throw sigrok::Error{SR_ERR};

		res->size = size;
		res->handle = file.release();
	} else {
		qWarning() << "AndroidAssetReader: Unknown resource type" << res->type;
		throw sigrok::Error{SR_ERR};
	}
}

void AndroidAssetReader::close(struct sr_resource *res)
{
	if (!res->handle) {
		qCritical("AndroidAssetReader: Invalid handle");
		throw sigrok::Error{SR_ERR_ARG};
	}
	const unique_ptr<QFile> file {static_cast<QFile*>(res->handle)};
	res->handle = nullptr;

	file->close();
}

size_t AndroidAssetReader::read(const struct sr_resource *res, void *buf, size_t count)
{
	if (!res->handle) {
		qCritical("AndroidAssetReader: Invalid handle");
		throw sigrok::Error{SR_ERR_ARG};
	}
	auto *const file = static_cast<QFile*>(res->handle);

	const auto n_read = file->read(static_cast<char*>(buf), count);
	if (n_read < 0)
		throw sigrok::Error{SR_ERR};

	return n_read;
}
