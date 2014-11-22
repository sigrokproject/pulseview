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

#ifndef PULSEVIEW_PV_VIEW_LOGICSIGNAL_H
#define PULSEVIEW_PV_VIEW_LOGICSIGNAL_H

#include "signal.hpp"

#include <memory>

class QToolBar;

namespace sigrok {
	class Device;
	class TriggerMatchType;
}

namespace pv {

namespace data {
class Logic;
}

namespace view {

class LogicSignal : public Signal
{
	Q_OBJECT

private:
	static const int SignalHeight;
	static const int SignalMargin;

	static const float Oversampling;

	static const QColor EdgeColour;
	static const QColor HighColour;
	static const QColor LowColour;

	static const QColor SignalColours[10];

public:
	LogicSignal(pv::Session &session,
		std::shared_ptr<sigrok::Device> device,
		std::shared_ptr<sigrok::Channel> channel,
		std::shared_ptr<pv::data::Logic> data);

	virtual ~LogicSignal();

	std::shared_ptr<pv::data::SignalData> data() const;

	std::shared_ptr<pv::data::Logic> logic_data() const;

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	std::pair<int, int> v_extents() const;

	/**
	 * Paints the background layer of the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
	void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the mid-layer of the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
	void paint_mid(QPainter &p, int left, int right);

private:
	void paint_caps(QPainter &p, QLineF *const lines,
		std::vector< std::pair<int64_t, bool> > &edges,
		bool level, double samples_per_pixel, double pixels_offset,
		float x_offset, float y_offset);

	void init_trigger_actions(QWidget *parent);

	QAction* match_action(const sigrok::TriggerMatchType *match);
	const sigrok::TriggerMatchType *action_match(QAction *action);
	void populate_popup_form(QWidget *parent, QFormLayout *form);
	void modify_trigger();

private Q_SLOTS:
	void on_trigger();

private:
	std::shared_ptr<sigrok::Device> device_;
	std::shared_ptr<pv::data::Logic> data_;

	const sigrok::TriggerMatchType *trigger_match_;
	QToolBar *trigger_bar_;
	QAction *trigger_none_;
	QAction *trigger_rising_;
	QAction *trigger_high_;
	QAction *trigger_falling_;
	QAction *trigger_low_;
	QAction *trigger_change_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_LOGICSIGNAL_H
