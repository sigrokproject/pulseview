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

#ifndef PULSEVIEW_PV_SIGNAL_H
#define PULSEVIEW_PV_SIGNAL_H

#include <QComboBox>
#include <QPen>
#include <QWidgetAction>

#include <stdint.h>

#include <libsigrok/libsigrok.h>

#include "trace.h"

namespace pv {

namespace data {
class SignalData;
}

namespace view {

class Signal : public Trace
{
	Q_OBJECT

private:
	static const QPen SignalAxisPen;

protected:
	Signal(pv::SigSession &session, const sr_probe *const probe);

public:
	virtual void init_context_bar_actions(QWidget *parent);

	/**
	 * Sets the name of the signal.
	 */
	void set_name(QString name);

	/**
	 * Returns true if the trace is visible and enabled.
	 */
	bool enabled() const;

protected:

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param y the y-offset of the axis.
	 * @param left the x-coordinate of the left edge of the view.
	 * @param right the x-coordinate of the right edge of the view.
	 */
	void paint_axis(QPainter &p, int y, int left, int right);

private slots:
	void on_text_changed(const QString &text);

protected:
	const sr_probe *const _probe;

	QWidgetAction *_name_action;
	QComboBox *_name_widget;
	bool _updating_name_widget;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_SIGNAL_H
