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

#include <QAction>
#include <QDebug>
#include <QHelpEvent>
#include <QToolTip>

#include "samplingbar.h"

#include <pv/devicemanager.h>
#include <pv/popups/deviceoptions.h>
#include <pv/popups/channels.h>
#include <pv/util.h>

#include <libsigrok/libsigrok.hpp>

using std::map;
using std::vector;
using std::max;
using std::min;
using std::shared_ptr;
using std::string;

using sigrok::Capability;
using sigrok::ConfigKey;
using sigrok::Device;
using sigrok::Error;

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
	_channels_button(this),
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
	setObjectName(QString::fromUtf8("SamplingBar"));

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

	_channels_button.setIcon(QIcon::fromTheme("channels",
		QIcon(":/icons/channels.svg")));

	_run_stop_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	addWidget(&_device_selector);
	_configure_button_action = addWidget(&_configure_button);
	addWidget(&_channels_button);
	addWidget(&_sample_count);
	addWidget(&_sample_rate);

	addWidget(&_run_stop_button);

	_sample_count.installEventFilter(this);
	_sample_rate.installEventFilter(this);
}

void SamplingBar::set_device_list(
	const std::map< shared_ptr<Device>, string > &device_names,
	shared_ptr<Device> selected)
{
	int selected_index = -1;

	assert(selected);

	_updating_device_selector = true;

	_device_selector.clear();

	for (auto entry : device_names) {
		auto device = entry.first;
		auto description = entry.second;

		assert(device);

		if (selected == device)
			selected_index = _device_selector.count();

		_device_selector.addItem(description.c_str(),
			qVariantFromValue(device));
	}

	// The selected device should have been in the list
	assert(selected_index != -1);
	_device_selector.setCurrentIndex(selected_index);

	update_device_config_widgets();

	_updating_device_selector = false;
}

shared_ptr<Device> SamplingBar::get_selected_device() const
{
	const int index = _device_selector.currentIndex();
	if (index < 0)
		return shared_ptr<Device>();

	return _device_selector.itemData(index).value<shared_ptr<Device>>();
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
	Glib::VariantContainerBase gvar_dict;
	GVariant *gvar_list;
	const uint64_t *elements = NULL;
	gsize num_elements;

	if (_updating_sample_rate)
		return;

	const shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	assert(!_updating_sample_rate);
	_updating_sample_rate = true;

	try {
		gvar_dict = device->config_list(ConfigKey::SAMPLERATE);
	} catch (Error error) {
		_sample_rate.show_none();
		_updating_sample_rate = false;
		return;
	}

	if ((gvar_list = g_variant_lookup_value(gvar_dict.gobj(),
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
	else if ((gvar_list = g_variant_lookup_value(gvar_dict.gobj(),
			"samplerates", G_VARIANT_TYPE("at"))))
	{
		elements = (const uint64_t *)g_variant_get_fixed_array(
				gvar_list, &num_elements, sizeof(uint64_t));
		_sample_rate.show_list(elements, num_elements);
		g_variant_unref(gvar_list);
	}
	_updating_sample_rate = false;

	update_sample_rate_selector_value();
}

void SamplingBar::update_sample_rate_selector_value()
{
	if (_updating_sample_rate)
		return;

	const shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	try {
		auto gvar = device->config_get(ConfigKey::SAMPLERATE);
		uint64_t samplerate =
			Glib::VariantBase::cast_dynamic<Glib::Variant<uint64_t>>(gvar).get();
		assert(!_updating_sample_rate);
		_updating_sample_rate = true;
		_sample_rate.set_value(samplerate);
		_updating_sample_rate = false;
	} catch (Error error) {
		qDebug() << "WARNING: Failed to get value of sample rate";
		return;
	}
}

void SamplingBar::update_sample_count_selector()
{
	if (_updating_sample_count)
		return;

	const shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	assert(!_updating_sample_count);
	_updating_sample_count = true;

	if (_sample_count_supported)
	{
		uint64_t sample_count = _sample_count.value();
		uint64_t min_sample_count = 0;
		uint64_t max_sample_count = MaxSampleCount;

		if (sample_count == 0)
			sample_count = DefaultSampleCount;

		try {
			auto gvar = device->config_list(ConfigKey::LIMIT_SAMPLES);
			g_variant_get(gvar.gobj(), "(tt)",
				&min_sample_count, &max_sample_count);
		} catch (Error error) {}

		min_sample_count = min(max(min_sample_count, MinSampleCount),
			max_sample_count);

		_sample_count.show_125_list(
			min_sample_count, max_sample_count);

		try {
			auto gvar = device->config_get(ConfigKey::LIMIT_SAMPLES);
			sample_count = g_variant_get_uint64(gvar.gobj());
			if (sample_count == 0)
				sample_count = DefaultSampleCount;
			sample_count = min(max(sample_count, MinSampleCount),
				max_sample_count);
		} catch (Error error) {}

		_sample_count.set_value(sample_count);
	}
	else
		_sample_count.show_none();

	_updating_sample_count = false;
}

void SamplingBar::update_device_config_widgets()
{
	using namespace pv::popups;

	const shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	// Update the configure popup
	DeviceOptions *const opts = new DeviceOptions(device, this);
	_configure_button_action->setVisible(
		!opts->binding().properties().empty());
	_configure_button.set_popup(opts);

	// Update the channels popup
	Channels *const channels = new Channels(_session, this);
	_channels_button.set_popup(channels);

	// Update supported options.
	_sample_count_supported = false;

	try {
		for (auto entry : device->config_keys(ConfigKey::DEVICE_OPTIONS))
		{
			auto key = entry.first;
			auto capabilities = entry.second;
			switch (key->id()) {
			case SR_CONF_LIMIT_SAMPLES:
				if (capabilities.count(Capability::SET))
					_sample_count_supported = true;
				break;
			case SR_CONF_LIMIT_FRAMES:
				if (capabilities.count(Capability::SET))
				{
					device->config_set(ConfigKey::LIMIT_FRAMES,
						Glib::Variant<uint64_t>::create(1));
					on_config_changed();
				}
				break;
			default:
				break;
			}
		}
	} catch (Error error) {}

	// Add notification of reconfigure events
	disconnect(this, SLOT(on_config_changed()));
	connect(&opts->binding(), SIGNAL(config_changed()),
		this, SLOT(on_config_changed()));

	// Update sweep timing widgets.
	update_sample_count_selector();
	update_sample_rate_selector();
}

void SamplingBar::commit_sample_count()
{
	uint64_t sample_count = 0;

	if (_updating_sample_count)
		return;

	const shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	sample_count = _sample_count.value();

	// Set the sample count
	assert(!_updating_sample_count);
	_updating_sample_count = true;
	if (_sample_count_supported)
	{
		try {
			device->config_set(ConfigKey::LIMIT_SAMPLES,
				Glib::Variant<uint64_t>::create(sample_count));
			on_config_changed();
		} catch (Error error) {
			qDebug() << "Failed to configure sample count.";
			return;
		}
	}
	_updating_sample_count = false;
}

void SamplingBar::commit_sample_rate()
{
	uint64_t sample_rate = 0;

	if (_updating_sample_rate)
		return;

	const shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	sample_rate = _sample_rate.value();
	if (sample_rate == 0)
		return;

	// Set the samplerate
	assert(!_updating_sample_rate);
	_updating_sample_rate = true;
	try {
		device->config_set(ConfigKey::SAMPLERATE,
			Glib::Variant<uint64_t>::create(sample_rate));
		on_config_changed();
	} catch (Error error) {
		qDebug() << "Failed to configure samplerate.";
		return;
	}
	_updating_sample_rate = false;
}

void SamplingBar::on_device_selected()
{
	if (_updating_device_selector)
		return;

	shared_ptr<Device> device = get_selected_device();
	if (!device)
		return;

	_session.set_device(device);

	update_device_config_widgets();
}

void SamplingBar::on_sample_count_changed()
{
	commit_sample_count();
}

void SamplingBar::on_sample_rate_changed()
{
	commit_sample_rate();
}

void SamplingBar::on_run_stop()
{
	commit_sample_count();
	commit_sample_rate();	
	run_stop();
}

void SamplingBar::on_config_changed()
{
	commit_sample_count();
	update_sample_count_selector();	
	commit_sample_rate();	
	update_sample_rate_selector();
}

bool SamplingBar::eventFilter(QObject *watched, QEvent *event)
{
	if ((watched == &_sample_count || watched == &_sample_rate) &&
		(event->type() == QEvent::ToolTip)) {
		double sec = (double)_sample_count.value() / _sample_rate.value();
		QHelpEvent *help_event = static_cast<QHelpEvent*>(event);

		QString str = tr("Total sampling time: %1").arg(pv::util::format_second(sec));
		QToolTip::showText(help_event->globalPos(), str);

		return true;
	}

	return false;
}

} // namespace toolbars
} // namespace pv
