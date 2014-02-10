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

#ifndef PULSEVIEW_PV_VIEW_SIGNAL_H
#define PULSEVIEW_PV_VIEW_SIGNAL_H

#include <boost/shared_ptr.hpp>

#include <QComboBox>
#include <QWidgetAction>

#include <stdint.h>

#include "trace.h"

struct sr_probe;

namespace pv {

class DevInst;

namespace data {
class SignalData;
}

namespace view {

class Signal : public Trace
{
	Q_OBJECT

protected:
	Signal(boost::shared_ptr<pv::DevInst> dev_inst, sr_probe *const probe);

public:
	/**
	 * Sets the name of the signal.
	 */
	void set_name(QString name);

	virtual boost::shared_ptr<pv::data::SignalData> data() const = 0;

	/**
	 * Returns true if the trace is visible and enabled.
	 */
	bool enabled() const;

	void enable(bool enable = true);

	const sr_probe* probe() const;

	virtual void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private slots:
	void on_disable();

protected:
	boost::shared_ptr<pv::DevInst> _dev_inst;
	sr_probe *const _probe;

	QComboBox *_name_widget;
	bool _updating_name_widget;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_SIGNAL_H
