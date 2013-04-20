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

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QToolBar>
#include <QToolButton>

struct st_dev_inst;
class QAction;

namespace pv {
namespace toolbars {

class SamplingBar : public QToolBar
{
	Q_OBJECT

private:
	static const uint64_t RecordLengths[20];
	static const uint64_t DefaultRecordLength;

public:
	SamplingBar(QWidget *parent);

	void set_device_list(const std::list<struct sr_dev_inst*> &devices);

	struct sr_dev_inst* get_selected_device() const;
	void set_selected_device(struct sr_dev_inst *const sdi);

	uint64_t get_record_length() const;

	void set_sampling(bool sampling);

signals:
	void run_stop();

private:
	void update_sample_rate_selector();
	void update_sample_rate_selector_value();
	void commit_sample_rate();

private slots:
	void on_device_selected();
	void on_sample_rate_changed();
	void on_configure();

private:
	QComboBox _device_selector;
	QToolButton _configure_button;

	QComboBox _record_length_selector;

	QComboBox _sample_rate_list;
	QAction *_sample_rate_list_action;
	QDoubleSpinBox _sample_rate_value;
	QAction *_sample_rate_value_action;

	QIcon _icon_green;
	QIcon _icon_grey;
	QToolButton _run_stop_button;
};

} // namespace toolbars
} // namespace pv

#endif // PULSEVIEW_PV_TOOLBARS_SAMPLINGBAR_H
