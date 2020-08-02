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

#ifndef PULSEVIEW_PV_DATA_MATHSIGNAL_HPP
#define PULSEVIEW_PV_DATA_MATHSIGNAL_HPP

#include <QSettings>
#include <QString>

#include <pv/data/analog.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/util.hpp>

using std::shared_ptr;

namespace pv {
class Session;

namespace data {

class SignalBase;
class SignalData;

class MathSignal : public SignalBase
{
	Q_OBJECT

private:
	static const int64_t ChunkLength;

public:
	MathSignal(pv::Session &session);
	virtual ~MathSignal();

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

Q_SIGNALS:
	void samples_cleared();

	void samples_added(uint64_t segment_id, uint64_t start_sample,
		uint64_t end_sample);

	void min_max_changed(float min, float max);

//private Q_SLOTS:
//	void on_data_cleared();
//	void on_data_received();

//	void on_samples_added(SharedPtrToSegment segment, uint64_t start_sample,
//		uint64_t end_sample);

//	void on_min_max_changed(float min, float max);

private:
	pv::Session &session_;

	QString error_message_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_MATHSIGNAL_HPP
