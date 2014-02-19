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

#ifndef PULSEVIEW_PV_TOOLBARS_SAMPLINGBAR_H
#define PULSEVIEW_PV_TOOLBARS_SAMPLINGBAR_H

#include <stdint.h>

#include <list>
#include <map>

#include <boost/shared_ptr.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QToolBar>
#include <QToolButton>

#include <pv/sigsession.h>
#include <pv/widgets/popuptoolbutton.h>
#include <pv/widgets/sweeptimingwidget.h>

class QAction;

namespace pv {

class SigSession;

namespace device {
class DevInst;
}

namespace toolbars {

class SamplingBar : public QToolBar
{
	Q_OBJECT

private:
	static const uint64_t MinSampleCount;
	static const uint64_t MaxSampleCount;
	static const uint64_t DefaultSampleCount;

public:
	SamplingBar(SigSession &session, QWidget *parent);

	void set_device_list(
		const std::list< boost::shared_ptr<pv::device::DevInst> >
			&devices);

	boost::shared_ptr<pv::device::DevInst> get_selected_device() const;
	void set_selected_device(
		boost::shared_ptr<pv::device::DevInst> dev_inst);

	void set_capture_state(pv::SigSession::capture_state state);

signals:
	void run_stop();

private:
	void update_sample_rate_selector();
	void update_sample_rate_selector_value();
	void update_sample_count_selector();
	void commit_sample_rate();
	void commit_sample_count();

private slots:
	void on_device_selected();
	void on_sample_count_changed();
	void on_sample_rate_changed();
	void on_run_stop();

	void on_config_changed();

private:
	SigSession &_session;

	QComboBox _device_selector;
	std::map<const sr_dev_inst*, boost::weak_ptr<device::DevInst> >
		_device_selector_map;
	bool _updating_device_selector;

	pv::widgets::PopupToolButton _configure_button;
	QAction *_configure_button_action;

	pv::widgets::PopupToolButton _probes_button;

	pv::widgets::SweepTimingWidget _sample_count;
	pv::widgets::SweepTimingWidget _sample_rate;
	bool _updating_sample_rate;
	bool _updating_sample_count;

	bool _sample_count_supported;

	QIcon _icon_red;
	QIcon _icon_green;
	QIcon _icon_grey;
	QToolButton _run_stop_button;
};

} // namespace toolbars
} // namespace pv

#endif // PULSEVIEW_PV_TOOLBARS_SAMPLINGBAR_H
