/*
 * This file is part of the sigrok project.
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

#include <assert.h>

extern "C" {
#include <libsigrok/libsigrok.h>
}

#include <QAction>

#include "samplingbar.h"

SamplingBar::SamplingBar(QWidget *parent) :
	QToolBar("Sampling Bar", parent),
	_device_selector(this),
	_sample_rate_list(this)
{
	connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
		this, SLOT(on_device_selected()));

	_sample_rate_value.setDecimals(0);
	_sample_rate_value.setSuffix("Hz");

	addWidget(&_device_selector);
	_sample_rate_list_action = addWidget(&_sample_rate_list);
	_sample_rate_value_action = addWidget(&_sample_rate_value);

	update_device_selector();
	update_sample_rate_selector();
}

struct sr_dev_inst* SamplingBar::get_selected_device() const
{
	const int index = _device_selector.currentIndex();
	if(index < 0)
		return NULL;

	return (sr_dev_inst*)_device_selector.itemData(
		index).value<void*>();
}

uint64_t SamplingBar::get_sample_rate() const
{
	assert(_sample_rate_value_action);
	assert(_sample_rate_list_action);

	if(_sample_rate_value_action->isVisible())
		return (uint64_t)_sample_rate_value.value();
	else if(_sample_rate_list_action->isVisible())
	{
		const int index = _device_selector.currentIndex();
		if(index < 0)
			return 0;
		
		return _device_selector.itemData(index).value<uint64_t>();
	}

	return 0;
}

void SamplingBar::update_device_selector()
{
	GSList *devices = NULL;

	/* Scan all drivers for all devices. */
	struct sr_dev_driver **const drivers = sr_driver_list();
	for (struct sr_dev_driver **driver = drivers; *driver; driver++) {
		GSList *tmpdevs = sr_driver_scan(*driver, NULL);
		for (GSList *l = tmpdevs; l; l = l->next)
			devices = g_slist_append(devices, l->data);
		g_slist_free(tmpdevs);
	}

	for (GSList *l = devices; l; l = l->next) {
		sr_dev_inst *const sdi = (sr_dev_inst*)l->data;

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

	g_slist_free(devices);
}

void SamplingBar::update_sample_rate_selector()
{
	const sr_dev_inst *const sdi = get_selected_device();
	const struct sr_samplerates *samplerates;

	assert(_sample_rate_value_action);
	assert(_sample_rate_list_action);

	if (sr_info_get(sdi->driver, SR_DI_SAMPLERATES,
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
}

void SamplingBar::on_device_selected()
{
	update_sample_rate_selector();
}
