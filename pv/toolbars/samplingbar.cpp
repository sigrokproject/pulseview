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

#include <pv/devicemanager.h>
#include <pv/popups/deviceoptions.h>
#include <pv/popups/probes.h>

using std::string;

namespace pv {
namespace toolbars {

const uint64_t SamplingBar::DefaultRecordLength = 1000000;

SamplingBar::SamplingBar(SigSession &session, QWidget *parent) :
	QToolBar("Sampling Bar", parent),
	_session(session),
	_device_selector(this),
	_updating_device_selector(false),
	_configure_button(this),
	_configure_button_action(NULL),
	_probes_button(this),
	_sample_count(" samples", this),
	_sample_rate("Hz", this),
	_updating_sample_rate(false),
	_updating_sample_count(false),
	_icon_red(":/icons/status-red.svg"),
	_icon_green(":/icons/status-green.svg"),
	_icon_grey(":/icons/status-grey.svg"),
	_run_stop_button(this)
{
	connect(&_run_stop_button, SIGNAL(clicked()),
		this, SLOT(on_run_stop()));
	connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
		this, SLOT(on_device_selected()));
	connect(&_sample_count, SIGNAL(value_changed()),
		this, SLOT(on_sample_count_changed()));
	connect(&_sample_rate, SIGNAL(value_changed()),
		this, SLOT(on_sample_rate_changed()));

	_sample_count.show_min_max_step(0, UINT64_MAX, 1);

	set_capture_state(pv::SigSession::Stopped);

	_configure_button.setIcon(QIcon::fromTheme("configure",
		QIcon(":/icons/configure.png")));

	_probes_button.setIcon(QIcon::fromTheme("probes",
		QIcon(":/icons/probes.svg")));

	_run_stop_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	addWidget(&_device_selector);
	_configure_button_action = addWidget(&_configure_button);
	addWidget(&_probes_button);
	addWidget(&_sample_count);
	addWidget(&_sample_rate);

	addWidget(&_run_stop_button);
}

void SamplingBar::set_device_list(
	const std::list<struct sr_dev_inst*> &devices)
{
	_updating_device_selector = true;

	_device_selector.clear();

	BOOST_FOREACH (sr_dev_inst *sdi, devices) {
		const string title = DeviceManager::format_device_title(sdi);
		_device_selector.addItem(title.c_str(),
			qVariantFromValue((void*)sdi));
	}

	_updating_device_selector = false;

	on_device_selected();
}

struct sr_dev_inst* SamplingBar::get_selected_device() const
{
	const int index = _device_selector.currentIndex();
	if (index < 0)
		return NULL;

	return (sr_dev_inst*)_device_selector.itemData(
		index).value<void*>();
}

void SamplingBar::set_selected_device(struct sr_dev_inst *const sdi)
{
	for (int i = 0; i < _device_selector.count(); i++)
		if (sdi == _device_selector.itemData(i).value<void*>()) {
			_device_selector.setCurrentIndex(i);
			return;
		}
}

uint64_t SamplingBar::get_record_length() const
{
	return _sample_count.value();
}

void SamplingBar::set_capture_state(pv::SigSession::capture_state state)
{
	const QIcon *icons[] = {&_icon_grey, &_icon_red, &_icon_green};
	_run_stop_button.setIcon(*icons[state]);
	_run_stop_button.setText((state == pv::SigSession::Stopped) ?
		tr("Run") : tr("Stop"));
}

void SamplingBar::update_sample_rate_selector()
{
	const sr_dev_inst *const sdi = get_selected_device();
	GVariant *gvar_dict, *gvar_list;
	const uint64_t *elements = NULL;
	gsize num_elements;

	if (!sdi)
		return;

	_updating_sample_rate = true;

	if (sr_config_list(sdi->driver, sdi, NULL,
			SR_CONF_SAMPLERATE, &gvar_dict) != SR_OK)
	{
		_sample_rate.show_none();
		_updating_sample_rate = false;
		return;
	}

	if ((gvar_list = g_variant_lookup_value(gvar_dict,
			"samplerate-steps", G_VARIANT_TYPE("at"))))
	{
		elements = (const uint64_t *)g_variant_get_fixed_array(
				gvar_list, &num_elements, sizeof(uint64_t));
		_sample_rate.show_min_max_step(elements[0], elements[1],
			elements[2]);
		g_variant_unref(gvar_list);
	}
	else if ((gvar_list = g_variant_lookup_value(gvar_dict,
			"samplerates", G_VARIANT_TYPE("at"))))
	{
		elements = (const uint64_t *)g_variant_get_fixed_array(
				gvar_list, &num_elements, sizeof(uint64_t));
		_sample_rate.show_list(elements, num_elements);
		g_variant_unref(gvar_list);
	}
	_updating_sample_rate = false;

	g_variant_unref(gvar_dict);
	update_sample_rate_selector_value();
}

void SamplingBar::update_sample_rate_selector_value()
{
	sr_dev_inst *const sdi = get_selected_device();
	GVariant *gvar;
	uint64_t samplerate;

	assert(sdi);

	if (sr_config_get(sdi->driver, sdi, NULL,
		SR_CONF_SAMPLERATE, &gvar) != SR_OK) {
		qDebug() <<
				"WARNING: Failed to get value of sample rate";
		return;
	}
	samplerate = g_variant_get_uint64(gvar);
	g_variant_unref(gvar);

	_updating_sample_rate = true;
	_sample_rate.set_value(samplerate);
	_updating_sample_rate = false;
}

void SamplingBar::update_sample_count_selector()
{
	sr_dev_inst *const sdi = get_selected_device();
	GVariant *gvar;
	uint64_t samplecount;

	assert(sdi);

	if (sr_config_get(sdi->driver, sdi, NULL,
		SR_CONF_LIMIT_SAMPLES, &gvar) != SR_OK)
	{
		_sample_count.show_none();
	}
	else
	{
		_sample_count.show_min_max_step(0, UINT64_MAX, 1);

		samplecount = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);

		_updating_sample_count = true;
		_sample_count.set_value(samplecount);
		_updating_sample_count = false;
	}
}

void SamplingBar::commit_sample_count()
{
	uint64_t sample_count = 0;

	sr_dev_inst *const sdi = get_selected_device();
	assert(sdi);

	sample_count = _sample_count.value();

	// Set the sample count
	if (sr_config_set(sdi, NULL, SR_CONF_LIMIT_SAMPLES,
		g_variant_new_uint64(sample_count)) != SR_OK) {
		qDebug() << "Failed to configure sample count.";
		return;
	}
}

void SamplingBar::commit_sample_rate()
{
	uint64_t sample_rate = 0;

	sr_dev_inst *const sdi = get_selected_device();
	assert(sdi);

	sample_rate = _sample_rate.value();
	if (sample_rate == 0)
		return;

	// Set the samplerate
	if (sr_config_set(sdi, NULL, SR_CONF_SAMPLERATE,
		g_variant_new_uint64(sample_rate)) != SR_OK) {
		qDebug() << "Failed to configure samplerate.";
		return;
	}
}

void SamplingBar::on_device_selected()
{
	using namespace pv::popups;

	if (_updating_device_selector)
		return;

	update_sample_count_selector();
	update_sample_rate_selector();

	if (_sample_count.value() == 0)
		_sample_count.set_value(DefaultRecordLength);

	sr_dev_inst *const sdi = get_selected_device();
	_session.set_device(sdi);

	// Update the configure popup
	DeviceOptions *const opts = new DeviceOptions(sdi, this);
	_configure_button_action->setVisible(
		!opts->binding().properties().empty());
	_configure_button.set_popup(opts);

	// Update the probes popup
	Probes *const probes = new Probes(_session, this);
	_probes_button.set_popup(probes);
}

void SamplingBar::on_sample_count_changed()
{
	if(!_updating_sample_count)
		commit_sample_count();
}

void SamplingBar::on_sample_rate_changed()
{
	if (!_updating_sample_rate)
		commit_sample_rate();
}

void SamplingBar::on_run_stop()
{
	commit_sample_rate();	
	run_stop();
}

} // namespace toolbars
} // namespace pv
