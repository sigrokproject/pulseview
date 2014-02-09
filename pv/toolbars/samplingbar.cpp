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
#include <pv/devinst.h>
#include <pv/popups/deviceoptions.h>
#include <pv/popups/probes.h>

using boost::shared_ptr;
using std::map;
using std::max;
using std::min;
using std::string;

namespace pv {
namespace toolbars {

const uint64_t SamplingBar::MinSampleCount = 100ULL;
const uint64_t SamplingBar::MaxSampleCount = 1000000000000ULL;
const uint64_t SamplingBar::DefaultSampleCount = 1000000;

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
	_sample_count_supported(false),
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
	const std::list< shared_ptr<pv::DevInst> > &devices)
{
	_updating_device_selector = true;

	_device_selector.clear();
	_device_selector_map.clear();

	BOOST_FOREACH (shared_ptr<pv::DevInst> dev_inst, devices) {
		assert(dev_inst);
		const string title = dev_inst->format_device_title();
		const sr_dev_inst *sdi = dev_inst->dev_inst();
		assert(sdi);

		_device_selector_map[sdi] = dev_inst;
		_device_selector.addItem(title.c_str(),
			qVariantFromValue((void*)sdi));
	}

	_updating_device_selector = false;

	on_device_selected();
}

shared_ptr<pv::DevInst> SamplingBar::get_selected_device() const
{
	const int index = _device_selector.currentIndex();
	if (index < 0)
		return shared_ptr<pv::DevInst>();

	const sr_dev_inst *const sdi =
		(const sr_dev_inst*)_device_selector.itemData(
			index).value<void*>();
	assert(sdi);

	map<const sr_dev_inst*, boost::weak_ptr<DevInst> >::
		const_iterator iter = _device_selector_map.find(sdi);
	if (iter == _device_selector_map.end())
		return shared_ptr<pv::DevInst>();

	return shared_ptr<pv::DevInst>((*iter).second);
}

void SamplingBar::set_selected_device(boost::shared_ptr<pv::DevInst> dev_inst)
{
	assert(dev_inst);

	for (int i = 0; i < _device_selector.count(); i++)
		if (dev_inst->dev_inst() ==
			_device_selector.itemData(i).value<void*>()) {
			_device_selector.setCurrentIndex(i);
			return;
		}
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
	GVariant *gvar_dict, *gvar_list;
	const uint64_t *elements = NULL;
	gsize num_elements;

	const shared_ptr<DevInst> dev_inst = get_selected_device();
	if (!dev_inst)
		return;

	const sr_dev_inst *const sdi = dev_inst->dev_inst();
	assert(sdi);

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

		const uint64_t min = elements[0];
		const uint64_t max = elements[1];
		const uint64_t step = elements[2];

		g_variant_unref(gvar_list);

		assert(min > 0);
		assert(max > 0);
		assert(max > min);
		assert(step > 0);

		if (step == 1)
			_sample_rate.show_125_list(min, max);
		else
		{
			// When the step is not 1, we cam't make a 1-2-5-10
			// list of sample rates, because we may not be able to
			// make round numbers. Therefore in this case, show a
			// spin box.
			_sample_rate.show_min_max_step(min, max, step);
		}
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
	GVariant *gvar;
	uint64_t samplerate;

	const shared_ptr<DevInst> dev_inst = get_selected_device();
	if (!dev_inst)
		return;

	const sr_dev_inst *const sdi = dev_inst->dev_inst();
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
	GVariant *gvar;

	const shared_ptr<DevInst> dev_inst = get_selected_device();
	if (!dev_inst)
		return;

	const sr_dev_inst *const sdi = dev_inst->dev_inst();
	assert(sdi);

	_updating_sample_count = true;

	if (_sample_count_supported)
	{
		uint64_t sample_count = DefaultSampleCount;
		uint64_t min_sample_count = 0;
		uint64_t max_sample_count = MaxSampleCount;

		if (sr_config_list(sdi->driver, sdi, NULL,
			SR_CONF_LIMIT_SAMPLES, &gvar) == SR_OK) {
			g_variant_get(gvar, "(tt)",
				&min_sample_count, &max_sample_count);
			g_variant_unref(gvar);
		}

		min_sample_count = min(max(min_sample_count, MinSampleCount),
			max_sample_count);

		_sample_count.show_125_list(
			min_sample_count, max_sample_count);

		if (sr_config_get(sdi->driver, sdi, NULL,
			SR_CONF_LIMIT_SAMPLES, &gvar) == SR_OK)
		{
			sample_count = g_variant_get_uint64(gvar);
			if (sample_count == 0)
				sample_count = DefaultSampleCount;
			sample_count = min(max(sample_count, MinSampleCount),
				max_sample_count);

			g_variant_unref(gvar);
		}

		_sample_count.set_value(sample_count);
	}
	else
		_sample_count.show_none();

	_updating_sample_count = false;
}

void SamplingBar::commit_sample_count()
{
	uint64_t sample_count = 0;

	const shared_ptr<DevInst> dev_inst = get_selected_device();
	if (!dev_inst)
		return;

	const sr_dev_inst *const sdi = dev_inst->dev_inst();
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

	const shared_ptr<DevInst> dev_inst = get_selected_device();
	if (!dev_inst)
		return;

	const sr_dev_inst *const sdi = dev_inst->dev_inst();
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
	GVariant *gvar;

	using namespace pv::popups;

	if (_updating_device_selector)
		return;

	const shared_ptr<DevInst> dev_inst = get_selected_device();
	if (!dev_inst)
		return;

	const sr_dev_inst *const sdi = dev_inst->dev_inst();
	assert(sdi);

	_session.set_device(dev_inst);

	// Update the configure popup
	DeviceOptions *const opts = new DeviceOptions(dev_inst, this);
	_configure_button_action->setVisible(
		!opts->binding().properties().empty());
	_configure_button.set_popup(opts);

	// Update the probes popup
	Probes *const probes = new Probes(_session, this);
	_probes_button.set_popup(probes);

	// Update supported options.
	_sample_count_supported = false;

	if (sr_config_list(sdi->driver, sdi, NULL,
		SR_CONF_DEVICE_OPTIONS, &gvar) == SR_OK)
	{
		gsize num_opts;
		const int *const options = (const int32_t *)g_variant_get_fixed_array(
	        gvar, &num_opts, sizeof(int32_t));
		for (unsigned int i = 0; i < num_opts; i++)
		{
			switch (options[i]) {
			case SR_CONF_LIMIT_SAMPLES:
				_sample_count_supported = true;
				break;
			case SR_CONF_LIMIT_FRAMES:
				sr_config_set(sdi, NULL, SR_CONF_LIMIT_FRAMES,
					g_variant_new_uint64(1));
				break;
			}
		}
	}

	// Update sweep timing widgets.
	update_sample_count_selector();
	update_sample_rate_selector();
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
	commit_sample_count();
	commit_sample_rate();	
	run_stop();
}

} // namespace toolbars
} // namespace pv
