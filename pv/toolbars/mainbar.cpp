/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012-2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <algorithm>
#include <cassert>

#include <QAction>
#include <QDebug>
#include <QHelpEvent>
#include <QMenu>
#include <QToolTip>

#include "mainbar.hpp"

#include <pv/devicemanager.hpp>
#include <pv/devices/hardwaredevice.hpp>
#include <pv/mainwindow.hpp>
#include <pv/popups/deviceoptions.hpp>
#include <pv/popups/channels.hpp>
#include <pv/util.hpp>
#include <pv/widgets/exportmenu.hpp>
#include <pv/widgets/importmenu.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::back_inserter;
using std::copy;
using std::list;
using std::map;
using std::max;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;

using sigrok::Capability;
using sigrok::ConfigKey;
using sigrok::Error;
using sigrok::InputFormat;

namespace pv {
namespace toolbars {

const uint64_t MainBar::MinSampleCount = 100ULL;
const uint64_t MainBar::MaxSampleCount = 1000000000000ULL;
const uint64_t MainBar::DefaultSampleCount = 1000000;

MainBar::MainBar(Session &session, MainWindow &main_window) :
	QToolBar("Sampling Bar", &main_window),
	session_(session),
	main_window_(main_window),
	device_selector_(this, session.device_manager(),
		main_window.action_connect()),
	configure_button_(this),
	configure_button_action_(nullptr),
	channels_button_(this),
	channels_button_action_(nullptr),
	sample_count_(" samples", this),
	sample_rate_("Hz", this),
	updating_sample_rate_(false),
	updating_sample_count_(false),
	sample_count_supported_(false),
	icon_red_(":/icons/status-red.svg"),
	icon_green_(":/icons/status-green.svg"),
	icon_grey_(":/icons/status-grey.svg"),
	run_stop_button_(this),
	run_stop_button_action_(nullptr),
	menu_button_(this)
{
	setObjectName(QString::fromUtf8("MainBar"));

	setMovable(false);
	setFloatable(false);
	setContextMenuPolicy(Qt::PreventContextMenu);

	// Open button
	QToolButton *const open_button = new QToolButton(this);

	widgets::ImportMenu *import_menu = new widgets::ImportMenu(this,
		session.device_manager().context(),
		main_window.action_open());
	connect(import_menu,
		SIGNAL(format_selected(std::shared_ptr<sigrok::InputFormat>)),
		&main_window_,
		SLOT(import_file(std::shared_ptr<sigrok::InputFormat>)));

	open_button->setMenu(import_menu);
	open_button->setDefaultAction(main_window.action_open());
	open_button->setPopupMode(QToolButton::MenuButtonPopup);

	// Save button
	QToolButton *const save_button = new QToolButton(this);

	widgets::ExportMenu *export_menu = new widgets::ExportMenu(this,
		session.device_manager().context(),
		main_window.action_save_as());
	connect(export_menu,
		SIGNAL(format_selected(std::shared_ptr<sigrok::OutputFormat>)),
		&main_window_,
		SLOT(export_file(std::shared_ptr<sigrok::OutputFormat>)));

	save_button->setMenu(export_menu);
	save_button->setDefaultAction(main_window.action_save_as());
	save_button->setPopupMode(QToolButton::MenuButtonPopup);

	// Device selector menu
	connect(&device_selector_, SIGNAL(device_selected()),
		this, SLOT(on_device_selected()));

	// Setup the decoder button
#ifdef ENABLE_DECODE
	QToolButton *add_decoder_button = new QToolButton(this);
	add_decoder_button->setIcon(QIcon::fromTheme("add-decoder",
		QIcon(":/icons/add-decoder.svg")));
	add_decoder_button->setPopupMode(QToolButton::InstantPopup);
	add_decoder_button->setMenu(main_window_.menu_decoder_add());
#endif

	// Setup the menu
	QMenu *const menu = new QMenu(this);

	QMenu *const menu_help = new QMenu;
	menu_help->setTitle(tr("&Help"));
	menu_help->addAction(main_window.action_about());

	menu->addAction(menu_help->menuAction());
	menu->addSeparator();
	menu->addAction(main_window.action_quit());

	menu_button_.setMenu(menu);
	menu_button_.setPopupMode(QToolButton::InstantPopup);
	menu_button_.setIcon(QIcon::fromTheme("menu",
		QIcon(":/icons/menu.svg")));

	// Setup the toolbar
	addWidget(open_button);
	addWidget(save_button);
	addSeparator();
	addAction(main_window.action_view_zoom_in());
	addAction(main_window.action_view_zoom_out());
	addAction(main_window.action_view_zoom_fit());
	addAction(main_window.action_view_zoom_one_to_one());
	addSeparator();
	addAction(main_window.action_view_show_cursors());
	addSeparator();

	connect(&run_stop_button_, SIGNAL(clicked()),
		this, SLOT(on_run_stop()));
	connect(&sample_count_, SIGNAL(value_changed()),
		this, SLOT(on_sample_count_changed()));
	connect(&sample_rate_, SIGNAL(value_changed()),
		this, SLOT(on_sample_rate_changed()));

	sample_count_.show_min_max_step(0, UINT64_MAX, 1);

	set_capture_state(pv::Session::Stopped);

	configure_button_.setIcon(QIcon::fromTheme("configure",
		QIcon(":/icons/configure.png")));

	channels_button_.setIcon(QIcon::fromTheme("channels",
		QIcon(":/icons/channels.svg")));

	run_stop_button_.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	addWidget(&device_selector_);
	configure_button_action_ = addWidget(&configure_button_);
	channels_button_action_ = addWidget(&channels_button_);
	addWidget(&sample_count_);
	addWidget(&sample_rate_);
	run_stop_button_action_ = addWidget(&run_stop_button_);
#ifdef ENABLE_DECODE
	addSeparator();
	addWidget(add_decoder_button);
#endif

	QWidget *const spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	addWidget(spacer);

	addWidget(&menu_button_);

	sample_count_.installEventFilter(this);
	sample_rate_.installEventFilter(this);
}

void MainBar::update_device_list()
{
	DeviceManager &mgr = session_.device_manager();
	shared_ptr<devices::Device> selected_device = session_.device();
	list< shared_ptr<devices::Device> > devs;

	copy(mgr.devices().begin(), mgr.devices().end(), back_inserter(devs));

	if (std::find(devs.begin(), devs.end(), selected_device) == devs.end())
		devs.push_back(selected_device);

	device_selector_.set_device_list(devs, selected_device);
	update_device_config_widgets();
}


void MainBar::set_capture_state(pv::Session::capture_state state)
{
	const QIcon *icons[] = {&icon_grey_, &icon_red_, &icon_green_};
	run_stop_button_.setIcon(*icons[state]);
	run_stop_button_.setText((state == pv::Session::Stopped) ?
		tr("Run") : tr("Stop"));
	run_stop_button_.setShortcut(QKeySequence(Qt::Key_Space));
}

void MainBar::update_sample_rate_selector()
{
	Glib::VariantContainerBase gvar_dict;
	GVariant *gvar_list;
	const uint64_t *elements = nullptr;
	gsize num_elements;
	map< const ConfigKey*, std::set<Capability> > keys;

	if (updating_sample_rate_) {
		sample_rate_.show_none();
		return;
	}

	const shared_ptr<devices::Device> device =
		device_selector_.selected_device();
	if (!device)
		return;

	assert(!updating_sample_rate_);
	updating_sample_rate_ = true;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	try {
		keys = sr_dev->config_keys(ConfigKey::DEVICE_OPTIONS);
	} catch (Error) {}

	const auto iter = keys.find(ConfigKey::SAMPLERATE);
	if (iter != keys.end() &&
		(*iter).second.find(sigrok::LIST) != (*iter).second.end()) {
		try {
			gvar_dict = sr_dev->config_list(ConfigKey::SAMPLERATE);
		} catch(const sigrok::Error &e) {
			// Failed to enunmerate samplerate
			(void)e;
		}
	}

	if (!gvar_dict.gobj()) {
		sample_rate_.show_none();
		updating_sample_rate_ = false;
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
			sample_rate_.show_125_list(min, max);
		else
		{
			// When the step is not 1, we cam't make a 1-2-5-10
			// list of sample rates, because we may not be able to
			// make round numbers. Therefore in this case, show a
			// spin box.
			sample_rate_.show_min_max_step(min, max, step);
		}
	}
	else if ((gvar_list = g_variant_lookup_value(gvar_dict.gobj(),
			"samplerates", G_VARIANT_TYPE("at"))))
	{
		elements = (const uint64_t *)g_variant_get_fixed_array(
				gvar_list, &num_elements, sizeof(uint64_t));
		sample_rate_.show_list(elements, num_elements);
		g_variant_unref(gvar_list);
	}
	updating_sample_rate_ = false;

	update_sample_rate_selector_value();
}

void MainBar::update_sample_rate_selector_value()
{
	if (updating_sample_rate_)
		return;

	const shared_ptr<devices::Device> device =
		device_selector_.selected_device();
	if (!device)
		return;

	try {
		auto gvar = device->device()->config_get(ConfigKey::SAMPLERATE);
		uint64_t samplerate =
			Glib::VariantBase::cast_dynamic<Glib::Variant<guint64>>(gvar).get();
		assert(!updating_sample_rate_);
		updating_sample_rate_ = true;
		sample_rate_.set_value(samplerate);
		updating_sample_rate_ = false;
	} catch (Error error) {
		qDebug() << "WARNING: Failed to get value of sample rate";
		return;
	}
}

void MainBar::update_sample_count_selector()
{
	if (updating_sample_count_)
		return;

	const shared_ptr<devices::Device> device =
		device_selector_.selected_device();
	if (!device)
		return;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	assert(!updating_sample_count_);
	updating_sample_count_ = true;

	if (!sample_count_supported_)
	{
		sample_count_.show_none();
		updating_sample_count_ = false;
		return;
	}

	uint64_t sample_count = sample_count_.value();
	uint64_t min_sample_count = 0;
	uint64_t max_sample_count = MaxSampleCount;

	if (sample_count == 0)
		sample_count = DefaultSampleCount;

	const auto keys = sr_dev->config_keys(ConfigKey::DEVICE_OPTIONS);
	const auto iter = keys.find(ConfigKey::LIMIT_SAMPLES);
	if (iter != keys.end() &&
		(*iter).second.find(sigrok::LIST) != (*iter).second.end()) {
		try {
			auto gvar =
				sr_dev->config_list(ConfigKey::LIMIT_SAMPLES);
			if (gvar.gobj())
				g_variant_get(gvar.gobj(), "(tt)",
					&min_sample_count, &max_sample_count);
		} catch(const sigrok::Error &e) {
			// Failed to query sample limit
			(void)e;
		}
	}

	min_sample_count = min(max(min_sample_count, MinSampleCount),
		max_sample_count);

	sample_count_.show_125_list(
		min_sample_count, max_sample_count);

	try {
		auto gvar = sr_dev->config_get(ConfigKey::LIMIT_SAMPLES);
		sample_count = g_variant_get_uint64(gvar.gobj());
		if (sample_count == 0)
			sample_count = DefaultSampleCount;
		sample_count = min(max(sample_count, MinSampleCount),
			max_sample_count);
	} catch (Error error) {}

	sample_count_.set_value(sample_count);

	updating_sample_count_ = false;
}

void MainBar::update_device_config_widgets()
{
	using namespace pv::popups;

	const shared_ptr<devices::Device> device =
		device_selector_.selected_device();

	// Hide the widgets if no device is selected
	channels_button_action_->setVisible(!!device);
	run_stop_button_action_->setVisible(!!device);
	if (!device) {
		configure_button_action_->setVisible(false);
		sample_count_.show_none();
		sample_rate_.show_none();
		return;
	}

	const shared_ptr<sigrok::Device> sr_dev = device->device();
	if (!sr_dev)
		return;

	// Update the configure popup
	DeviceOptions *const opts = new DeviceOptions(sr_dev, this);
	configure_button_action_->setVisible(
		!opts->binding().properties().empty());
	configure_button_.set_popup(opts);

	// Update the channels popup
	Channels *const channels = new Channels(session_, this);
	channels_button_.set_popup(channels);

	// Update supported options.
	sample_count_supported_ = false;

	try {
		for (auto entry : sr_dev->config_keys(ConfigKey::DEVICE_OPTIONS))
		{
			auto key = entry.first;
			auto capabilities = entry.second;
			switch (key->id()) {
			case SR_CONF_LIMIT_SAMPLES:
				if (capabilities.count(Capability::SET))
					sample_count_supported_ = true;
				break;
			case SR_CONF_LIMIT_FRAMES:
				if (capabilities.count(Capability::SET))
				{
					sr_dev->config_set(ConfigKey::LIMIT_FRAMES,
						Glib::Variant<guint64>::create(1));
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

void MainBar::commit_sample_count()
{
	uint64_t sample_count = 0;

	const shared_ptr<devices::Device> device =
		device_selector_.selected_device();
	if (!device)
		return;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	sample_count = sample_count_.value();
	if (sample_count_supported_)
	{
		try {
			sr_dev->config_set(ConfigKey::LIMIT_SAMPLES,
				Glib::Variant<guint64>::create(sample_count));
			update_sample_count_selector();
		} catch (Error error) {
			qDebug() << "Failed to configure sample count.";
			return;
		}
	}

	// Devices with built-in memory might impose limits on certain
	// configurations, so let's check what sample rate the driver
	// lets us use now.
	update_sample_rate_selector();
}

void MainBar::commit_sample_rate()
{
	uint64_t sample_rate = 0;

	const shared_ptr<devices::Device> device =
		device_selector_.selected_device();
	if (!device)
		return;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	sample_rate = sample_rate_.value();
	if (sample_rate == 0)
		return;

	try {
		sr_dev->config_set(ConfigKey::SAMPLERATE,
			Glib::Variant<guint64>::create(sample_rate));
		update_sample_rate_selector();
	} catch (Error error) {
		qDebug() << "Failed to configure samplerate.";
		return;
	}

	// Devices with built-in memory might impose limits on certain
	// configurations, so let's check what sample count the driver
	// lets us use now.
	update_sample_count_selector();
}

void MainBar::on_device_selected()
{
	shared_ptr<devices::Device> device = device_selector_.selected_device();
	if (!device)
		return;

	main_window_.select_device(device);

	update_device_config_widgets();
}

void MainBar::on_sample_count_changed()
{
	if (!updating_sample_count_)
		commit_sample_count();
}

void MainBar::on_sample_rate_changed()
{
	if (!updating_sample_rate_)
		commit_sample_rate();
}

void MainBar::on_run_stop()
{
	commit_sample_count();
	commit_sample_rate();	
	main_window_.run_stop();
}

void MainBar::on_config_changed()
{
	commit_sample_count();
	commit_sample_rate();	
}

bool MainBar::eventFilter(QObject *watched, QEvent *event)
{
	if ((watched == &sample_count_ || watched == &sample_rate_) &&
		(event->type() == QEvent::ToolTip)) {
		double sec = (double)sample_count_.value() / sample_rate_.value();
		QHelpEvent *help_event = static_cast<QHelpEvent*>(event);

		QString str = tr("Total sampling time: %1").arg(pv::util::format_second(sec));
		QToolTip::showText(help_event->globalPos(), str);

		return true;
	}

	return false;
}

} // namespace toolbars
} // namespace pv
