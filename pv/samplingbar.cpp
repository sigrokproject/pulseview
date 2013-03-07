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

#include <extdef.h>

#include <assert.h>

#include <boost/foreach.hpp>

#include <libsigrok/libsigrok.h>

#include <QAction>
#include <QDebug>

#include "samplingbar.h"

#include <pv/dialogs/deviceoptions.h>

namespace pv {

const uint64_t SamplingBar::RecordLengths[20] = {
	1000,
	2500,
	5000,
	10000,
	25000,
	50000,
	100000,
	250000,
	500000,
	1000000,
	2000000,
	5000000,
	10000000,
	25000000,
	50000000,
	100000000,
	250000000,
	500000000,
	1000000000,
	10000000000ULL,
};

const uint64_t SamplingBar::DefaultRecordLength = 1000000;

SamplingBar::SamplingBar(QWidget *parent) :
	QToolBar("Sampling Bar", parent),
	_device_selector(this),
	_configure_button(this),
	_record_length_selector(this),
	_sample_rate_list(this),
	_icon_green(":/icons/status-green.svg"),
	_icon_grey(":/icons/status-grey.svg"),
	_run_stop_button(this)
{
	connect(&_run_stop_button, SIGNAL(clicked()),
		this, SIGNAL(run_stop()));
	connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
		this, SLOT(on_device_selected()));
	connect(&_configure_button, SIGNAL(clicked()),
		this, SLOT(configure()));

	_sample_rate_value.setDecimals(0);
	_sample_rate_value.setSuffix("Hz");

	for (size_t i = 0; i < countof(RecordLengths); i++)
	{
		const uint64_t &l = RecordLengths[i];
		char *const text = sr_si_string_u64(l, " samples");
		_record_length_selector.addItem(QString(text),
			qVariantFromValue(l));
		g_free(text);

		if (l == DefaultRecordLength)
			_record_length_selector.setCurrentIndex(i);
	}

	set_sampling(false);

	_configure_button.setIcon(QIcon::fromTheme("configure",
		QIcon(":/icons/configure.png")));

	_run_stop_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	addWidget(&_device_selector);
	addWidget(&_configure_button);
	addWidget(&_record_length_selector);
	_sample_rate_list_action = addWidget(&_sample_rate_list);
	_sample_rate_value_action = addWidget(&_sample_rate_value);
	addWidget(&_run_stop_button);

	connect(&_sample_rate_list, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_sample_rate_changed()));
	connect(&_sample_rate_value, SIGNAL(editingFinished()),
		this, SLOT(on_sample_rate_changed()));
}

void SamplingBar::set_device_list(
	const std::list<struct sr_dev_inst*> &devices)
{
	_device_selector.clear();

	BOOST_FOREACH (sr_dev_inst *sdi, devices) {
		QString title;
		if (sdi->vendor && sdi->vendor[0])
			title += sdi->vendor + QString(" ");
		if (sdi->model && sdi->model[0])
			title += sdi->model + QString(" ");
		if (sdi->version && sdi->version[0])
			title += sdi->version + QString(" ");

		_device_selector.addItem(title, qVariantFromValue(
			(void*)sdi));
	}

	update_sample_rate_selector();
}

struct sr_dev_inst* SamplingBar::get_selected_device() const
{
	const int index = _device_selector.currentIndex();
	if (index < 0)
		return NULL;

	return (sr_dev_inst*)_device_selector.itemData(
		index).value<void*>();
}

uint64_t SamplingBar::get_record_length() const
{
	const int index = _record_length_selector.currentIndex();
	if (index < 0)
		return 0;

	return _record_length_selector.itemData(index).value<uint64_t>();
}

void SamplingBar::set_sampling(bool sampling)
{
	_run_stop_button.setIcon(sampling ? _icon_green : _icon_grey);
	_run_stop_button.setText(sampling ? "Stop" : "Run");
}

void SamplingBar::update_sample_rate_selector()
{
	const sr_dev_inst *const sdi = get_selected_device();
	const struct sr_samplerates *samplerates;

	assert(_sample_rate_value_action);
	assert(_sample_rate_list_action);

	if (!sdi)
		return;

	if (sr_config_list(sdi->driver, SR_CONF_SAMPLERATE,
		(const void **)&samplerates, sdi) != SR_OK)
		return;

	_sample_rate_list_action->setVisible(false);
	_sample_rate_value_action->setVisible(false);

	if (samplerates->step)
	{
		_sample_rate_value.setRange(
			samplerates->low, samplerates->high);
		_sample_rate_value.setSingleStep(samplerates->step);
		_sample_rate_value_action->setVisible(true);
	}
	else
	{
		_sample_rate_list.clear();
		for (const uint64_t *rate = samplerates->list;
		     *rate; rate++)
			_sample_rate_list.addItem(
				sr_samplerate_string(*rate),
				qVariantFromValue(*rate));
		_sample_rate_list.show();
		_sample_rate_list_action->setVisible(true);
	}

	update_sample_rate_selector_value();
}

void SamplingBar::update_sample_rate_selector_value()
{
	sr_dev_inst *const sdi = get_selected_device();
	assert(sdi);

	uint64_t *samplerate = NULL;
	if(sr_config_get(sdi->driver, SR_CONF_SAMPLERATE,
		(const void**)&samplerate, sdi) != SR_OK) {
		qDebug() <<
				"WARNING: Failed to get value of sample rate";
		return;
	}

	assert(_sample_rate_value_action);
	assert(_sample_rate_list_action);

	if (_sample_rate_value_action->isVisible())
		_sample_rate_value.setValue(*samplerate);
	else if (_sample_rate_list_action->isVisible())
	{
		for(int i = 0; i < _sample_rate_list.count(); i++)
			if(*samplerate == _sample_rate_list.itemData(
				i).value<uint64_t>())
				_sample_rate_list.setCurrentIndex(i);
	}
}

void SamplingBar::commit_sample_rate()
{
	uint64_t sample_rate = 0;

	sr_dev_inst *const sdi = get_selected_device();
	assert(sdi);

	assert(_sample_rate_value_action);
	assert(_sample_rate_list_action);

	if (_sample_rate_value_action->isVisible())
		sample_rate = (uint64_t)_sample_rate_value.value();
	else if (_sample_rate_list_action->isVisible())
	{
		const int index = _sample_rate_list.currentIndex();
		if (index >= 0)
			sample_rate = _sample_rate_list.itemData(
				index).value<uint64_t>();
	}

	// Set the samplerate
	if (sr_config_set(sdi, SR_CONF_SAMPLERATE,
		&sample_rate) != SR_OK) {
		qDebug() << "Failed to configure samplerate.";
		return;
	}
}

void SamplingBar::on_device_selected()
{
	update_sample_rate_selector();
}

void SamplingBar::on_sample_rate_changed()
{
	commit_sample_rate();
}

void SamplingBar::configure()
{
	commit_sample_rate();

	sr_dev_inst *const sdi = get_selected_device();
	assert(sdi);

	pv::dialogs::DeviceOptions dlg(this, sdi);
	dlg.exec();

	update_sample_rate_selector_value();
}

} // namespace pv
