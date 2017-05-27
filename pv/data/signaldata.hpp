/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DATA_SIGNALDATA_HPP
#define PULSEVIEW_PV_DATA_SIGNALDATA_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include <QObject>

using std::shared_ptr;
using std::vector;

namespace pv {
namespace data {

class Segment;

class SignalData : public QObject
{
	Q_OBJECT

public:
	SignalData() = default;
	virtual ~SignalData() = default;

public:
	virtual vector< shared_ptr<Segment> > segments() const = 0;

	virtual void clear() = 0;

	virtual uint64_t max_sample_count() const = 0;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_SIGNALDATA_HPP
