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

#ifndef SAMPLINGBAR_H
#define SAMPLINGBAR_H

#include <stdint.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QToolBar>
#include <QToolButton>

class QAction;

class SamplingBar : public QToolBar
{
	Q_OBJECT

private:
	static const uint64_t RecordLengths[11];

public:
	SamplingBar(QWidget *parent);

	struct sr_dev_inst* get_selected_device() const;
	uint64_t get_record_length() const;
	uint64_t get_sample_rate() const;

signals:
	void run_stop();

private:
	void update_device_selector();
	void update_sample_rate_selector();

private slots:
	void on_device_selected();

private:
	QComboBox _device_selector;

	QComboBox _record_length_selector;

	QComboBox _sample_rate_list;
	QAction *_sample_rate_list_action;
	QDoubleSpinBox _sample_rate_value;
	QAction *_sample_rate_value_action;

	QToolButton _run_stop_button;
};

#endif // SAMPLINGBAR_H
