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

#ifndef PULSEVIEW_PV_DATA_ANALOG_HPP
#define PULSEVIEW_PV_DATA_ANALOG_HPP

#include "signaldata.hpp"

#include <deque>
#include <memory>

#include <QObject>
#include <QColor>
#include "pv/data/segment.hpp"
#include "pv/data/signalbase.hpp"

using std::deque;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace data {

class AnalogSegment;

class Analog : public SignalData
{
	Q_OBJECT

public:
	/**
	 * Types and statics used with templates
	 */
	typedef AnalogSegment segment_t;
	typedef float sample_t;
	static const QColor SignalColors[8];
	static const char *InvalidSignal;
	static const SignalBase::ChannelType math_channel_type = SignalBase::AnalogMathChannel;

public:
	Analog();

	void push_segment(shared_ptr<AnalogSegment> &segment);

	const deque< shared_ptr<AnalogSegment> >& analog_segments() const;

	inline const deque< shared_ptr<AnalogSegment> >& typed_segments() const
	{
		return analog_segments();
	}

	vector< shared_ptr<Segment> > segments() const;

	uint32_t get_segment_count() const;

	void clear();

	void set_samplerate(double value);

	double get_samplerate() const;

	uint64_t max_sample_count() const;

	void notify_samples_added(shared_ptr<Segment> segment, uint64_t start_sample,
		uint64_t end_sample);

	void notify_min_max_changed(float min, float max);

Q_SIGNALS:
	void samples_cleared();

	void samples_added(SharedPtrToSegment segment, uint64_t start_sample,
		uint64_t end_sample);

	void min_max_changed(float min, float max);

private Q_SLOTS:
	void on_segment_completed();

private:
	double samplerate_;
	deque< shared_ptr<AnalogSegment> > segments_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_ANALOG_HPP
