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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_VIEW_ANALOGSIGNAL_HPP
#define PULSEVIEW_PV_VIEW_ANALOGSIGNAL_HPP

#include "signal.hpp"

#include <memory>

namespace pv {

namespace data {
class Analog;
class AnalogSegment;
}

namespace view {

class AnalogSignal : public Signal
{
private:
	static const int NominalHeight;
	static const QColor SignalColours[4];

	static const float EnvelopeThreshold;

public:
	AnalogSignal(pv::Session &session,
		std::shared_ptr<sigrok::Channel> channel,
		std::shared_ptr<pv::data::Analog> data);

	virtual ~AnalogSignal();

	std::shared_ptr<pv::data::SignalData> data() const;

	std::shared_ptr<pv::data::Analog> analog_data() const;

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	std::pair<int, int> v_extents() const;

	/**
	 * Paints the background layer of the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with..
	 **/
	void paint_back(QPainter &p, const ViewItemPaintParams &pp);

	/**
	 * Paints the mid-layer of the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with..
	 **/
	void paint_mid(QPainter &p, const ViewItemPaintParams &pp);

private:
	void paint_trace(QPainter &p,
		const std::shared_ptr<pv::data::AnalogSegment> &segment,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

	void paint_envelope(QPainter &p,
		const std::shared_ptr<pv::data::AnalogSegment> &segment,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

private:
	std::shared_ptr<pv::data::Analog> data_;
	float scale_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_ANALOGSIGNAL_HPP
