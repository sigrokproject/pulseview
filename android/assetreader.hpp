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

#ifndef PULSEVIEW_ANDROID_ASSETREADER_HPP
#define PULSEVIEW_ANDROID_ASSETREADER_HPP

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::string;

namespace pv {

class AndroidAssetReader : public sigrok::ResourceReader
{
public:
	AndroidAssetReader() {}
	virtual ~AndroidAssetReader();

private:
	void open(struct sr_resource *res, string name) override;
	void close(struct sr_resource *res) override;
	size_t read(const struct sr_resource *res, void *buf, size_t count) override;
};

} // namespace pv

#endif // !PULSEVIEW_ANDROID_ASSETREADER_HPP
