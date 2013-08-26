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

#ifndef PULSEVIEW_PV_DECODESIGNAL_H
#define PULSEVIEW_PV_DECODESIGNAL_H

#include "trace.h"

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Decoder;
}

namespace view {

class DecodeSignal : public Trace
{
	Q_OBJECT

public:
	DecodeSignal(pv::SigSession &session,
		boost::shared_ptr<pv::data::Decoder> decoder);

	void init_context_bar_actions(QWidget *parent);

	bool enabled() const;

	void set_view(pv::view::View *view);

	/**
	 * Paints the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	void paint(QPainter &p, int left, int right);

	const std::list<QAction*> get_context_bar_actions();

private:

	/**
	 * When painting into the rectangle, calculate the y
	 * offset of the zero point.
	 **/
	int get_nominal_offset(const QRect &rect) const;

private:
	boost::shared_ptr<pv::data::Decoder> _decoder;

	uint64_t _decode_start, _decode_end;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_DECODESIGNAL_H
