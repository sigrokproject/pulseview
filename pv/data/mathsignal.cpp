/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
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

#include <limits>

#include <QDebug>

#include "mathsignal.hpp"

#include <pv/globalsettings.hpp>
#include <pv/session.hpp>
#include <pv/data/analogsegment.hpp>
#include <pv/data/signalbase.hpp>

using std::make_shared;

namespace pv {
namespace data {

const int64_t MathSignal::ChunkLength = 256 * 1024;


MathSignal::MathSignal(pv::Session &session) :
	SignalBase(nullptr, SignalBase::MathChannel),
	session_(session)
{
	shared_ptr<data::Analog> data(new data::Analog());
	set_data(data);

	shared_ptr<data::AnalogSegment> segment = make_shared<data::AnalogSegment>(
		*data, data->get_segment_count(), session.get_samplerate());

	data->push_segment(segment);
}

MathSignal::~MathSignal()
{
}

void MathSignal::save_settings(QSettings &settings) const
{
	(void)settings;
}

void MathSignal::restore_settings(QSettings &settings)
{
	(void)settings;
}

} // namespace data
} // namespace pv
