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

#ifndef PULSEVIEW_PV_LOGICSIGNAL_H
#define PULSEVIEW_PV_LOGICSIGNAL_H

#include "signal.h"

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
}

namespace view {

class LogicSignal : public Signal
{
	Q_OBJECT

private:
	static const float Oversampling;

	static const QColor EdgeColour;
	static const QColor HighColour;
	static const QColor LowColour;

	static const QColor SignalColours[10];

public:
	LogicSignal(pv::SigSession &session, const sr_probe *const probe,
		boost::shared_ptr<pv::data::Logic> data);

	virtual ~LogicSignal();

	const std::list<QAction*> get_context_bar_actions();

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param y the y-coordinate to draw the signal at.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	void paint(QPainter &p, int y, int left, int right, double scale,
		double offset);

private:

	void paint_caps(QPainter &p, QLineF *const lines,
		std::vector< std::pair<int64_t, bool> > &edges,
		bool level, double samples_per_pixel, double pixels_offset,
		float x_offset, float y_offset);

	static void add_trigger_action(const char *trig_types, char type,
		QAction *action, std::list<QAction*> &actions);

	void set_trigger(char type);

private slots:
	void on_trigger_none();
	void on_trigger_rising();
	void on_trigger_high();
	void on_trigger_falling();
	void on_trigger_low();
	void on_trigger_change();

private:
	boost::shared_ptr<pv::data::Logic> _data;

	QAction _separator;

	QIcon _icon_trigger_none;
	QAction _trigger_none;
	QIcon _icon_trigger_rising;
	QAction _trigger_rising;
	QIcon _icon_trigger_high;
	QAction _trigger_high;
	QIcon _icon_trigger_falling;
	QAction _trigger_falling;
	QIcon _icon_trigger_low;
	QAction _trigger_low;
	QIcon _icon_trigger_change;
	QAction _trigger_change;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_LOGICSIGNAL_H
