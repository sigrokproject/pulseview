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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <extdef.h>

#include <algorithm>
#include <cassert>

#include <QAction>
#include <QDebug>
#include <QFileDialog>
#include <QHelpEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QToolTip>

#include "mainbar.hpp"

#include <boost/algorithm/string/join.hpp>

#include <pv/data/decodesignal.hpp>
#include <pv/devicemanager.hpp>
#include <pv/devices/hardwaredevice.hpp>
#include <pv/devices/inputfile.hpp>
#include <pv/devices/sessionfile.hpp>
#include <pv/dialogs/connect.hpp>
#include <pv/dialogs/inputoutputoptions.hpp>
#include <pv/dialogs/storeprogress.hpp>
#include <pv/mainwindow.hpp>
#include <pv/popups/channels.hpp>
#include <pv/popups/deviceoptions.hpp>
#include <pv/util.hpp>
#include <pv/views/trace/view.hpp>
#include <pv/widgets/exportmenu.hpp>
#include <pv/widgets/importmenu.hpp>
#ifdef ENABLE_DECODE
#include <pv/widgets/decodermenu.hpp>
#endif

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::back_inserter;
using std::copy;
using std::list;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

using sigrok::Capability;
using sigrok::ConfigKey;
using sigrok::Error;
using sigrok::InputFormat;
using sigrok::OutputFormat;

using boost::algorithm::join;

namespace pv {
namespace toolbars {

const uint64_t MainBar::MinSampleCount = 100ULL;
const uint64_t MainBar::MaxSampleCount = 1000000000000ULL;
const uint64_t MainBar::DefaultSampleCount = 1000000;

const char *MainBar::SettingOpenDirectory = "MainWindow/OpenDirectory";
const char *MainBar::SettingSaveDirectory = "MainWindow/SaveDirectory";

MainBar::MainBar(Session &session, QWidget *parent, pv::views::trace::View *view) :
	StandardBar(session, parent, view, false),
	action_new_view_(new QAction(this)),
	action_open_(new QAction(this)),
	action_save_as_(new QAction(this)),
	action_save_selection_as_(new QAction(this)),
	action_connect_(new QAction(this)),
	open_button_(new QToolButton()),
	save_button_(new QToolButton()),
	device_selector_(parent, session.device_manager(), action_connect_),
	configure_button_(this),
	configure_button_action_(nullptr),
	channels_button_(this),
	channels_button_action_(nullptr),
	sample_count_(" samples", this),
	sample_rate_("Hz", this),
	updating_sample_rate_(false),
	updating_sample_count_(false),
	sample_count_supported_(false)
#ifdef ENABLE_DECODE
	, add_decoder_button_(new QToolButton()),
	menu_decoders_add_(new pv::widgets::DecoderMenu(this, true))
#endif
{
	setObjectName(QString::fromUtf8("MainBar"));

	setContextMenuPolicy(Qt::PreventContextMenu);

	// Actions
	action_new_view_->setText(tr("New &View"));
	action_new_view_->setIcon(QIcon::fromTheme("window-new",
		QIcon(":/icons/window-new.png")));
	connect(action_new_view_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionNewView_triggered()));

	action_open_->setText(tr("&Open..."));
	action_open_->setIcon(QIcon::fromTheme("document-open",
		QIcon(":/icons/document-open.png")));
	action_open_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
	connect(action_open_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionOpen_triggered()));

	action_save_as_->setText(tr("&Save As..."));
	action_save_as_->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	action_save_as_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	connect(action_save_as_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSaveAs_triggered()));

	action_save_selection_as_->setText(tr("Save Selected &Range As..."));
	action_save_selection_as_->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	action_save_selection_as_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	connect(action_save_selection_as_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSaveSelectionAs_triggered()));

	widgets::ExportMenu *menu_file_export = new widgets::ExportMenu(this,
		session.device_manager().context());
	menu_file_export->setTitle(tr("&Export"));
	connect(menu_file_export, SIGNAL(format_selected(shared_ptr<sigrok::OutputFormat>)),
		this, SLOT(export_file(shared_ptr<sigrok::OutputFormat>)));

	widgets::ImportMenu *menu_file_import = new widgets::ImportMenu(this,
		session.device_manager().context());
	menu_file_import->setTitle(tr("&Import"));
	connect(menu_file_import, SIGNAL(format_selected(shared_ptr<sigrok::InputFormat>)),
		this, SLOT(import_file(shared_ptr<sigrok::InputFormat>)));

	action_connect_->setText(tr("&Connect to Device..."));
	connect(action_connect_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionConnect_triggered()));

	// Open button
	widgets::ImportMenu *import_menu = new widgets::ImportMenu(this,
		session.device_manager().context(), action_open_);
	connect(import_menu, SIGNAL(format_selected(shared_ptr<sigrok::InputFormat>)),
		this, SLOT(import_file(shared_ptr<sigrok::InputFormat>)));

	open_button_->setMenu(import_menu);
	open_button_->setDefaultAction(action_open_);
	open_button_->setPopupMode(QToolButton::MenuButtonPopup);

	// Save button
	vector<QAction*> open_actions;
	open_actions.push_back(action_save_as_);
	open_actions.push_back(action_save_selection_as_);

	widgets::ExportMenu *export_menu = new widgets::ExportMenu(this,
		session.device_manager().context(), open_actions);
	connect(export_menu, SIGNAL(format_selected(shared_ptr<sigrok::OutputFormat>)),
		this, SLOT(export_file(shared_ptr<sigrok::OutputFormat>)));

	save_button_->setMenu(export_menu);
	save_button_->setDefaultAction(action_save_as_);
	save_button_->setPopupMode(QToolButton::MenuButtonPopup);

	// Device selector menu
	connect(&device_selector_, SIGNAL(device_selected()),
		this, SLOT(on_device_selected()));

	// Setup the decoder button
#ifdef ENABLE_DECODE
	menu_decoders_add_->setTitle(tr("&Add"));
	connect(menu_decoders_add_, SIGNAL(decoder_selected(srd_decoder*)),
		this, SLOT(add_decoder(srd_decoder*)));

	add_decoder_button_->setIcon(QIcon(":/icons/add-decoder.svg"));
	add_decoder_button_->setPopupMode(QToolButton::InstantPopup);
	add_decoder_button_->setMenu(menu_decoders_add_);
	add_decoder_button_->setToolTip(tr("Add low-level, non-stacked protocol decoder"));
#endif

	connect(&sample_count_, SIGNAL(value_changed()),
		this, SLOT(on_sample_count_changed()));
	connect(&sample_rate_, SIGNAL(value_changed()),
		this, SLOT(on_sample_rate_changed()));

	sample_count_.show_min_max_step(0, UINT64_MAX, 1);

	set_capture_state(pv::Session::Stopped);

	configure_button_.setToolTip(tr("Configure Device"));
	configure_button_.setIcon(QIcon::fromTheme("preferences-system",
		QIcon(":/icons/preferences-system.png")));

	channels_button_.setToolTip(tr("Configure Channels"));
	channels_button_.setIcon(QIcon(":/icons/channels.svg"));

	add_toolbar_widgets();

	sample_count_.installEventFilter(this);
	sample_rate_.installEventFilter(this);

	// Setup session_ events
	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(on_capture_state_changed(int)));
	connect(&session, SIGNAL(device_changed()),
		this, SLOT(on_device_changed()));

	update_device_list();
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
	bool ui_enabled = (state == pv::Session::Stopped) ? true : false;

	device_selector_.setEnabled(ui_enabled);
	configure_button_.setEnabled(ui_enabled);
	channels_button_.setEnabled(ui_enabled);
	sample_count_.setEnabled(ui_enabled);
	sample_rate_.setEnabled(ui_enabled);
}

void MainBar::reset_device_selector()
{
	device_selector_.reset();
}

QAction* MainBar::action_open() const
{
	return action_open_;
}

QAction* MainBar::action_save_as() const
{
	return action_save_as_;
}

QAction* MainBar::action_save_selection_as() const
{
	return action_save_selection_as_;
}

QAction* MainBar::action_connect() const
{
	return action_connect_;
}

void MainBar::update_sample_rate_selector()
{
	Glib::VariantContainerBase gvar_dict;
	GVariant *gvar_list;
	const uint64_t *elements = nullptr;
	gsize num_elements;
	map< const ConfigKey*, set<Capability> > keys;

	if (updating_sample_rate_) {
		sample_rate_.show_none();
		return;
	}

	const shared_ptr<devices::Device> device = device_selector_.selected_device();
	if (!device)
		return;

	assert(!updating_sample_rate_);
	updating_sample_rate_ = true;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	sample_rate_.allow_user_entered_values(false);
	if (sr_dev->config_check(ConfigKey::EXTERNAL_CLOCK, Capability::GET)) {
		try {
			auto gvar = sr_dev->config_get(ConfigKey::EXTERNAL_CLOCK);
			if (gvar.gobj()) {
				bool value = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(
					gvar).get();
				sample_rate_.allow_user_entered_values(value);
			}
		} catch (Error& error) {
			// Do nothing
		}
	}


	if (sr_dev->config_check(ConfigKey::SAMPLERATE, Capability::LIST)) {
		try {
			gvar_dict = sr_dev->config_list(ConfigKey::SAMPLERATE);
		} catch (Error& error) {
			qDebug() << tr("Failed to get sample rate list:") << error.what();
		}
	} else {
		sample_rate_.show_none();
		updating_sample_rate_ = false;
		return;
	}

	if ((gvar_list = g_variant_lookup_value(gvar_dict.gobj(),
			"samplerate-steps", G_VARIANT_TYPE("at")))) {
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
		else {
			// When the step is not 1, we cam't make a 1-2-5-10
			// list of sample rates, because we may not be able to
			// make round numbers. Therefore in this case, show a
			// spin box.
			sample_rate_.show_min_max_step(min, max, step);
		}
	} else if ((gvar_list = g_variant_lookup_value(gvar_dict.gobj(),
			"samplerates", G_VARIANT_TYPE("at")))) {
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

	const shared_ptr<devices::Device> device = device_selector_.selected_device();
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
	} catch (Error& error) {
		qDebug() << tr("Failed to get value of sample rate:") << error.what();
	}
}

void MainBar::update_sample_count_selector()
{
	if (updating_sample_count_)
		return;

	const shared_ptr<devices::Device> device = device_selector_.selected_device();
	if (!device)
		return;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	assert(!updating_sample_count_);
	updating_sample_count_ = true;

	if (!sample_count_supported_) {
		sample_count_.show_none();
		updating_sample_count_ = false;
		return;
	}

	uint64_t sample_count = sample_count_.value();
	uint64_t min_sample_count = 0;
	uint64_t max_sample_count = MaxSampleCount;
	bool default_count_set = false;

	if (sample_count == 0) {
		sample_count = DefaultSampleCount;
		default_count_set = true;
	}

	if (sr_dev->config_check(ConfigKey::LIMIT_SAMPLES, Capability::LIST)) {
		try {
			auto gvar = sr_dev->config_list(ConfigKey::LIMIT_SAMPLES);
			if (gvar.gobj())
				g_variant_get(gvar.gobj(), "(tt)",
					&min_sample_count, &max_sample_count);
		} catch (Error& error) {
			qDebug() << tr("Failed to get sample limit list:") << error.what();
		}
	}

	min_sample_count = min(max(min_sample_count, MinSampleCount),
		max_sample_count);

	sample_count_.show_125_list(min_sample_count, max_sample_count);

	if (sr_dev->config_check(ConfigKey::LIMIT_SAMPLES, Capability::GET)) {
		auto gvar = sr_dev->config_get(ConfigKey::LIMIT_SAMPLES);
		sample_count = g_variant_get_uint64(gvar.gobj());
		if (sample_count == 0) {
			sample_count = DefaultSampleCount;
			default_count_set = true;
		}
		sample_count = min(max(sample_count, MinSampleCount),
			max_sample_count);
	}

	sample_count_.set_value(sample_count);

	updating_sample_count_ = false;

	// If we show the default rate then make sure the device uses the same
	if (default_count_set)
		commit_sample_count();
}

void MainBar::update_device_config_widgets()
{
	using namespace pv::popups;

	const shared_ptr<devices::Device> device = device_selector_.selected_device();

	// Hide the widgets if no device is selected
	channels_button_action_->setVisible(!!device);
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
	configure_button_action_->setVisible(!opts->binding().properties().empty());
	configure_button_.set_popup(opts);

	// Update the channels popup
	Channels *const channels = new Channels(session_, this);
	channels_button_.set_popup(channels);

	// Update supported options.
	sample_count_supported_ = false;

	if (sr_dev->config_check(ConfigKey::LIMIT_SAMPLES, Capability::SET))
		sample_count_supported_ = true;

	// Add notification of reconfigure events
	disconnect(this, SLOT(on_config_changed()));
	connect(&opts->binding(), SIGNAL(config_changed()),
		this, SLOT(on_config_changed()));

	// Update sweep timing widgets.
	update_sample_count_selector();
	update_sample_rate_selector();
}

void MainBar::commit_sample_rate()
{
	uint64_t sample_rate = 0;

	const shared_ptr<devices::Device> device = device_selector_.selected_device();
	if (!device)
		return;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	sample_rate = sample_rate_.value();

	try {
		sr_dev->config_set(ConfigKey::SAMPLERATE,
			Glib::Variant<guint64>::create(sample_rate));
		update_sample_rate_selector();
	} catch (Error& error) {
		qDebug() << tr("Failed to configure samplerate:") << error.what();
		return;
	}

	// Devices with built-in memory might impose limits on certain
	// configurations, so let's check what sample count the driver
	// lets us use now.
	update_sample_count_selector();
}

void MainBar::commit_sample_count()
{
	uint64_t sample_count = 0;

	const shared_ptr<devices::Device> device = device_selector_.selected_device();
	if (!device)
		return;

	const shared_ptr<sigrok::Device> sr_dev = device->device();

	sample_count = sample_count_.value();
	if (sample_count_supported_) {
		try {
			sr_dev->config_set(ConfigKey::LIMIT_SAMPLES,
				Glib::Variant<guint64>::create(sample_count));
			update_sample_count_selector();
		} catch (Error& error) {
			qDebug() << tr("Failed to configure sample count:") << error.what();
			return;
		}
	}

	// Devices with built-in memory might impose limits on certain
	// configurations, so let's check what sample rate the driver
	// lets us use now.
	update_sample_rate_selector();
}

void MainBar::show_session_error(const QString text, const QString info_text)
{
	QMessageBox msg(this);
	msg.setText(text);
	msg.setInformativeText(info_text);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void MainBar::add_decoder(srd_decoder *decoder)
{
#ifdef ENABLE_DECODE
	assert(decoder);
	shared_ptr<data::DecodeSignal> signal = session_.add_decode_signal();
	if (signal)
		signal->stack_decoder(decoder);
#else
	(void)decoder;
#endif
}

void MainBar::export_file(shared_ptr<OutputFormat> format, bool selection_only)
{
	using pv::dialogs::StoreProgress;

	// Stop any currently running capture session
	session_.stop_capture();

	QSettings settings;
	const QString dir = settings.value(SettingSaveDirectory).toString();

	pair<uint64_t, uint64_t> sample_range;

	// Selection only? Verify that the cursors are active and fetch their values
	if (selection_only) {
		views::trace::View *trace_view =
			qobject_cast<views::trace::View*>(session_.main_view().get());

		if (!trace_view->cursors()->enabled()) {
			show_session_error(tr("Missing Cursors"), tr("You need to set the " \
				"cursors before you can save the data enclosed by them " \
				"to a session file (e.g. using the Show Cursors button)."));
			return;
		}

		const double samplerate = session_.get_samplerate();

		const pv::util::Timestamp& start_time = trace_view->cursors()->first()->time();
		const pv::util::Timestamp& end_time = trace_view->cursors()->second()->time();

		const uint64_t start_sample = (uint64_t)max(
			0.0, start_time.convert_to<double>() * samplerate);
		const uint64_t end_sample = (uint64_t)max(
			0.0, end_time.convert_to<double>() * samplerate);

		if ((start_sample == 0) && (end_sample == 0)) {
			// Both cursors are negative and were clamped to 0
			show_session_error(tr("Invalid Range"), tr("The cursors don't " \
				"define a valid range of samples."));
			return;
		}

		sample_range = make_pair(start_sample, end_sample);
	} else {
		sample_range = make_pair(0, 0);
	}

	// Construct the filter
	const vector<string> exts = format->extensions();
	QString filter = tr("%1 files ").arg(
		QString::fromStdString(format->description()));

	if (exts.empty())
		filter += "(*)";
	else
		filter += QString("(*.%1);;%2 (*)").arg(
			QString::fromStdString(join(exts, ", *.")),
			tr("All Files"));

	// Show the file dialog
	const QString file_name = QFileDialog::getSaveFileName(
		this, tr("Save File"), dir, filter);

	if (file_name.isEmpty())
		return;

	const QString abs_path = QFileInfo(file_name).absolutePath();
	settings.setValue(SettingSaveDirectory, abs_path);

	// Show the options dialog
	map<string, Glib::VariantBase> options;
	if (!format->options().empty()) {
		dialogs::InputOutputOptions dlg(
			tr("Export %1").arg(QString::fromStdString(
				format->description())),
			format->options(), this);
		if (!dlg.exec())
			return;
		options = dlg.options();
	}

	if (!selection_only)
		session_.set_name(QFileInfo(file_name).fileName());

	StoreProgress *dlg = new StoreProgress(file_name, format, options,
		sample_range, session_, this);
	dlg->run();
}

void MainBar::import_file(shared_ptr<InputFormat> format)
{
	assert(format);

	QSettings settings;
	const QString dir = settings.value(SettingOpenDirectory).toString();

	// Construct the filter
	const vector<string> exts = format->extensions();
	const QString filter_exts = exts.empty() ? "" : QString::fromStdString("%1 (%2)").arg(
		tr("%1 files").arg(QString::fromStdString(format->description())),
		QString::fromStdString("*.%1").arg(QString::fromStdString(join(exts, " *."))));
	const QString filter_all = QString::fromStdString("%1 (%2)").arg(
		tr("All Files"), QString::fromStdString("*"));
	const QString filter = QString::fromStdString("%1%2%3").arg(
		exts.empty() ? "" : filter_exts,
		exts.empty() ? "" : ";;",
		filter_all);

	// Show the file dialog
	const QString file_name = QFileDialog::getOpenFileName(
		this, tr("Import File"), dir, filter);

	if (file_name.isEmpty())
		return;

	// Show the options dialog
	map<string, Glib::VariantBase> options;
	if (!format->options().empty()) {
		dialogs::InputOutputOptions dlg(
			tr("Import %1").arg(QString::fromStdString(
				format->description())),
			format->options(), this);
		if (!dlg.exec())
			return;
		options = dlg.options();
	}

	session_.load_file(file_name, format, options);

	const QString abs_path = QFileInfo(file_name).absolutePath();
	settings.setValue(SettingOpenDirectory, abs_path);
}

void MainBar::on_device_selected()
{
	shared_ptr<devices::Device> device = device_selector_.selected_device();

	if (device)
		session_.select_device(device);
	else
		reset_device_selector();
}

void MainBar::on_device_changed()
{
	update_device_list();
	update_device_config_widgets();
}

void MainBar::on_capture_state_changed(int state)
{
	set_capture_state((pv::Session::capture_state)state);
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

void MainBar::on_config_changed()
{
	// We want to also call update_sample_rate_selector() here in case
	// the user changed the SR_CONF_EXTERNAL_CLOCK option. However,
	// commit_sample_rate() does this already, so we don't call it here

	commit_sample_count();
	commit_sample_rate();
}

void MainBar::on_actionNewView_triggered()
{
	new_view(&session_);
}

void MainBar::on_actionOpen_triggered()
{
	QSettings settings;
	const QString dir = settings.value(SettingOpenDirectory).toString();

	// Show the dialog
	const QString file_name = QFileDialog::getOpenFileName(
		this, tr("Open File"), dir, tr(
			"sigrok Sessions (*.sr);;"
			"All Files (*)"));

	if (!file_name.isEmpty()) {
		session_.load_file(file_name);

		const QString abs_path = QFileInfo(file_name).absolutePath();
		settings.setValue(SettingOpenDirectory, abs_path);
	}
}

void MainBar::on_actionSaveAs_triggered()
{
	export_file(session_.device_manager().context()->output_formats()["srzip"]);
}

void MainBar::on_actionSaveSelectionAs_triggered()
{
	export_file(session_.device_manager().context()->output_formats()["srzip"], true);
}

void MainBar::on_actionConnect_triggered()
{
	// Stop any currently running capture session
	session_.stop_capture();

	dialogs::Connect dlg(this, session_.device_manager());

	// If the user selected a device, select it in the device list. Select the
	// current device otherwise.
	if (dlg.exec())
		session_.select_device(dlg.get_selected_device());

	update_device_list();
}

void MainBar::add_toolbar_widgets()
{
	addAction(action_new_view_);
	addSeparator();
	addWidget(open_button_);
	addWidget(save_button_);
	addSeparator();

	StandardBar::add_toolbar_widgets();

	addWidget(&device_selector_);
	configure_button_action_ = addWidget(&configure_button_);
	channels_button_action_ = addWidget(&channels_button_);
	addWidget(&sample_count_);
	addWidget(&sample_rate_);
#ifdef ENABLE_DECODE
	addSeparator();
	addWidget(add_decoder_button_);
#endif
}

bool MainBar::eventFilter(QObject *watched, QEvent *event)
{
	if (sample_count_supported_ && (watched == &sample_count_ ||
		watched == &sample_rate_) &&
		(event->type() == QEvent::ToolTip)) {

		auto sec = pv::util::Timestamp(sample_count_.value()) / sample_rate_.value();
		QHelpEvent *help_event = static_cast<QHelpEvent*>(event);

		QString str = tr("Total sampling time: %1").arg(
			pv::util::format_time_si(sec, pv::util::SIPrefix::unspecified, 0, "s", false));
		QToolTip::showText(help_event->globalPos(), str);

		return true;
	}

	return false;
}

} // namespace toolbars
} // namespace pv
