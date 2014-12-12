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
#include <memory>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QToolBar>
#include <QToolButton>

#include <pv/session.hpp>
#include <pv/widgets/popuptoolbutton.hpp>
#include <pv/widgets/sweeptimingwidget.hpp>

namespace sigrok {
	class Device;
}

Q_DECLARE_METATYPE(std::shared_ptr<sigrok::Device>)

class QAction;

namespace pv {

class MainWindow;
class Session;

namespace toolbars {

class SamplingBar : public QToolBar
{
	Q_OBJECT

private:
	static const uint64_t MinSampleCount;
	static const uint64_t MaxSampleCount;
	static const uint64_t DefaultSampleCount;

public:
	SamplingBar(Session &session, pv::MainWindow &main_window);

	void set_device_list(
		const std::list< std::shared_ptr<sigrok::Device> > &devices,
		std::shared_ptr<sigrok::Device> selected);

	std::shared_ptr<sigrok::Device> get_selected_device() const;

	void set_capture_state(pv::Session::capture_state state);

Q_SIGNALS:
	void run_stop();

private:
	void update_sample_rate_selector();
	void update_sample_rate_selector_value();
	void update_sample_count_selector();
	void update_device_config_widgets();
	void commit_sample_rate();
	void commit_sample_count();

private Q_SLOTS:
	void on_device_selected();
	void on_sample_count_changed();
	void on_sample_rate_changed();
	void on_run_stop();

	void on_config_changed();

protected:
	bool eventFilter(QObject *watched, QEvent *event);

private:
	Session &session_;
	MainWindow &main_window_;

	QComboBox device_selector_;
	bool updating_device_selector_;

	pv::widgets::PopupToolButton configure_button_;
	QAction *configure_button_action_;

	pv::widgets::PopupToolButton channels_button_;

	pv::widgets::SweepTimingWidget sample_count_;
	pv::widgets::SweepTimingWidget sample_rate_;
	bool updating_sample_rate_;
	bool updating_sample_count_;

	bool sample_count_supported_;

	QIcon icon_red_;
	QIcon icon_green_;
	QIcon icon_grey_;
	QToolButton run_stop_button_;
};

} // namespace toolbars
} // namespace pv

#endif // PULSEVIEW_PV_TOOLBARS_SAMPLINGBAR_H
