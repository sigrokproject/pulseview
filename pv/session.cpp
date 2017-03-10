/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012-14 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifdef _WIN32
// Windows: Avoid boost/thread namespace pollution (which includes windows.h).
#define NOGDI
#define NORESOURCE
#endif
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <QFileInfo>

#include <cassert>
#include <mutex>
#include <stdexcept>

#include <sys/stat.h>

#include "session.hpp"
#include "devicemanager.hpp"

#include "data/analog.hpp"
#include "data/analogsegment.hpp"
#include "data/decoderstack.hpp"
#include "data/logic.hpp"
#include "data/logicsegment.hpp"
#include "data/signalbase.hpp"
#include "data/decode/decoder.hpp"

#include "devices/hardwaredevice.hpp"
#include "devices/inputfile.hpp"
#include "devices/sessionfile.hpp"

#include "toolbars/mainbar.hpp"

#include "view/analogsignal.hpp"
#include "view/decodetrace.hpp"
#include "view/logicsignal.hpp"
#include "view/signal.hpp"
#include "view/view.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

using boost::shared_lock;
using boost::shared_mutex;
using boost::unique_lock;

using std::dynamic_pointer_cast;
using std::function;
using std::lock_guard;
using std::list;
using std::map;
using std::mutex;
using std::pair;
using std::recursive_mutex;
using std::set;
using std::shared_ptr;
using std::make_shared;
using std::string;
using std::unordered_set;
using std::vector;

using sigrok::Analog;
using sigrok::Channel;
using sigrok::ChannelType;
using sigrok::ConfigKey;
using sigrok::DatafeedCallbackFunction;
using sigrok::Error;
using sigrok::Header;
using sigrok::InputFormat;
using sigrok::Logic;
using sigrok::Meta;
using sigrok::OutputFormat;
using sigrok::Packet;
using sigrok::PacketPayload;
using sigrok::Session;
using sigrok::SessionDevice;

using Glib::VariantBase;
using Glib::Variant;

namespace pv {
Session::Session(DeviceManager &device_manager, QString name) :
	device_manager_(device_manager),
	default_name_(name),
	name_(name),
	capture_state_(Stopped),
	cur_samplerate_(0),
	data_saved_(true)
{
}

Session::~Session()
{
	// Stop and join to the thread
	stop_capture();
}

DeviceManager& Session::device_manager()
{
	return device_manager_;
}

const DeviceManager& Session::device_manager() const
{
	return device_manager_;
}

shared_ptr<sigrok::Session> Session::session() const
{
	if (!device_)
		return shared_ptr<sigrok::Session>();
	return device_->session();
}

shared_ptr<devices::Device> Session::device() const
{
	return device_;
}

QString Session::name() const
{
	return name_;
}

void Session::set_name(QString name)
{
	if (default_name_.isEmpty())
		default_name_ = name;

	name_ = name;

	name_changed();
}

const std::list< std::shared_ptr<views::ViewBase> > Session::views() const
{
	return views_;
}

std::shared_ptr<views::ViewBase> Session::main_view() const
{
	return main_view_;
}

void Session::set_main_bar(std::shared_ptr<pv::toolbars::MainBar> main_bar)
{
	main_bar_ = main_bar;
}

shared_ptr<pv::toolbars::MainBar> Session::main_bar() const
{
	return main_bar_;
}

bool Session::data_saved() const
{
	return data_saved_;
}

void Session::save_settings(QSettings &settings) const
{
	map<string, string> dev_info;
	list<string> key_list;
	int stacks = 0, views = 0;

	if (device_) {
		shared_ptr<devices::HardwareDevice> hw_device =
			dynamic_pointer_cast< devices::HardwareDevice >(device_);

		if (hw_device) {
			settings.setValue("device_type", "hardware");
			settings.beginGroup("device");

			key_list.emplace_back("vendor");
			key_list.emplace_back("model");
			key_list.emplace_back("version");
			key_list.emplace_back("serial_num");
			key_list.emplace_back("connection_id");

			dev_info = device_manager_.get_device_info(device_);

			for (string key : key_list) {
				if (dev_info.count(key))
					settings.setValue(QString::fromUtf8(key.c_str()),
							QString::fromUtf8(dev_info.at(key).c_str()));
				else
					settings.remove(QString::fromUtf8(key.c_str()));
			}

			settings.endGroup();
		}

		shared_ptr<devices::SessionFile> sessionfile_device =
			dynamic_pointer_cast< devices::SessionFile >(device_);

		if (sessionfile_device) {
			settings.setValue("device_type", "sessionfile");
			settings.beginGroup("device");
			settings.setValue("filename", QString::fromStdString(
				sessionfile_device->full_name()));
			settings.endGroup();
		}

		// Save channels and decoders
		for (shared_ptr<data::SignalBase> base : signalbases_) {
#ifdef ENABLE_DECODE
			if (base->is_decode_signal()) {
				shared_ptr<pv::data::DecoderStack> decoder_stack =
						base->decoder_stack();
				std::shared_ptr<data::decode::Decoder> top_decoder =
						decoder_stack->stack().front();

				settings.beginGroup("decoder_stack" + QString::number(stacks++));
				settings.setValue("id", top_decoder->decoder()->id);
				settings.setValue("name", top_decoder->decoder()->name);
				settings.endGroup();
			} else
#endif
			{
				settings.beginGroup(base->internal_name());
				base->save_settings(settings);
				settings.endGroup();
			}
		}

		settings.setValue("decoder_stacks", stacks);

		// Save view states and their signal settings
		// Note: main_view must be saved as view0
		settings.beginGroup("view" + QString::number(views++));
		main_view_->save_settings(settings);
		settings.endGroup();

		for (shared_ptr<views::ViewBase> view : views_) {
			if (view != main_view_) {
				settings.beginGroup("view" + QString::number(views++));
				view->save_settings(settings);
				settings.endGroup();
			}
		}

		settings.setValue("views", views);
	}
}

void Session::restore_settings(QSettings &settings)
{
	shared_ptr<devices::Device> device;

	QString device_type = settings.value("device_type").toString();

	if (device_type == "hardware") {
		map<string, string> dev_info;
		list<string> key_list;

		// Re-select last used device if possible but only if it's not demo
		settings.beginGroup("device");
		key_list.emplace_back("vendor");
		key_list.emplace_back("model");
		key_list.emplace_back("version");
		key_list.emplace_back("serial_num");
		key_list.emplace_back("connection_id");

		for (string key : key_list) {
			const QString k = QString::fromStdString(key);
			if (!settings.contains(k))
				continue;

			const string value = settings.value(k).toString().toStdString();
			if (!value.empty())
				dev_info.insert(std::make_pair(key, value));
		}

		if (dev_info.count("model") > 0)
			device = device_manager_.find_device_from_info(dev_info);

		if (device)
			set_device(device);

		settings.endGroup();
	}

	if (device_type == "sessionfile") {
		settings.beginGroup("device");
		QString filename = settings.value("filename").toString();
		settings.endGroup();

		if (QFileInfo(filename).isReadable()) {
			device = make_shared<devices::SessionFile>(device_manager_.context(),
				filename.toStdString());
			set_device(device);

			// TODO Perform error handling
			start_capture([](QString infoMessage) { (void)infoMessage; });

			set_name(QFileInfo(filename).fileName());
		}
	}

	if (device) {
		// Restore channels
		for (shared_ptr<data::SignalBase> base : signalbases_) {
			settings.beginGroup(base->internal_name());
			base->restore_settings(settings);
			settings.endGroup();
		}

		// Restore decoders
#ifdef ENABLE_DECODE
		int stacks = settings.value("decoder_stacks").toInt();

		for (int i = 0; i < stacks; i++) {
			settings.beginGroup("decoder_stack" + QString::number(i++));

			QString id = settings.value("id").toString();
			add_decoder(srd_decoder_get_by_id(id.toStdString().c_str()));

			settings.endGroup();
		}
#endif

		// Restore views
		int views = settings.value("views").toInt();

		for (int i = 0; i < views; i++) {
			settings.beginGroup("view" + QString::number(i));

			if (i > 0) {
				views::ViewType type = (views::ViewType)settings.value("type").toInt();
				add_view(name_, type, this);
				views_.back()->restore_settings(settings);
			} else
				main_view_->restore_settings(settings);

			settings.endGroup();
		}
	}
}

void Session::select_device(shared_ptr<devices::Device> device)
{
	try {
		if (device)
			set_device(device);
		else
			set_default_device();
	} catch (const QString &e) {
		main_bar_->session_error(tr("Failed to Select Device"),
			tr("Failed to Select Device"));
	}
}

void Session::set_device(shared_ptr<devices::Device> device)
{
	assert(device);

	// Ensure we are not capturing before setting the device
	stop_capture();

	if (device_)
		device_->close();

	device_.reset();

	// Revert name back to default name (e.g. "Session 1") as the data is gone
	name_ = default_name_;
	name_changed();

	// Remove all stored data
	for (std::shared_ptr<views::ViewBase> view : views_) {
		view->clear_signals();
#ifdef ENABLE_DECODE
		view->clear_decode_signals();
#endif
	}
	for (const shared_ptr<data::SignalData> d : all_signal_data_)
		d->clear();
	all_signal_data_.clear();
	signalbases_.clear();
	cur_logic_segment_.reset();

	for (auto entry : cur_analog_segments_) {
		shared_ptr<sigrok::Channel>(entry.first).reset();
		shared_ptr<data::AnalogSegment>(entry.second).reset();
	}

	logic_data_.reset();

	signals_changed();

	device_ = std::move(device);

	try {
		device_->open();
	} catch (const QString &e) {
		device_.reset();
	}

	if (device_) {
		device_->session()->add_datafeed_callback([=]
			(shared_ptr<sigrok::Device> device, shared_ptr<Packet> packet) {
				data_feed_in(device, packet);
			});

		update_signals();
	}

	device_changed();
}

void Session::set_default_device()
{
	const list< shared_ptr<devices::HardwareDevice> > &devices =
		device_manager_.devices();

	if (devices.empty())
		return;

	// Try and find the demo device and select that by default
	const auto iter = std::find_if(devices.begin(), devices.end(),
		[] (const shared_ptr<devices::HardwareDevice> &d) {
			return d->hardware_device()->driver()->name() ==
			"demo";	});
	set_device((iter == devices.end()) ? devices.front() : *iter);
}

void Session::load_init_file(const std::string &file_name,
	const std::string &format)
{
	shared_ptr<InputFormat> input_format;

	if (!format.empty()) {
		const map<string, shared_ptr<InputFormat> > formats =
			device_manager_.context()->input_formats();
		const auto iter = find_if(formats.begin(), formats.end(),
			[&](const pair<string, shared_ptr<InputFormat> > f) {
				return f.first == format; });
		if (iter == formats.end()) {
			main_bar_->session_error(tr("Error"),
				tr("Unexpected input format: %s").arg(QString::fromStdString(format)));
			return;
		}

		input_format = (*iter).second;
	}

	load_file(QString::fromStdString(file_name), input_format);
}

void Session::load_file(QString file_name,
	std::shared_ptr<sigrok::InputFormat> format,
	const std::map<std::string, Glib::VariantBase> &options)
{
	const QString errorMessage(
		QString("Failed to load file %1").arg(file_name));

	try {
		if (format)
			set_device(shared_ptr<devices::Device>(
				new devices::InputFile(
					device_manager_.context(),
					file_name.toStdString(),
					format, options)));
		else
			set_device(shared_ptr<devices::Device>(
				new devices::SessionFile(
					device_manager_.context(),
					file_name.toStdString())));
	} catch (Error e) {
		main_bar_->session_error(tr("Failed to load ") + file_name, e.what());
		set_default_device();
		main_bar_->update_device_list();
		return;
	}

	main_bar_->update_device_list();

	start_capture([&, errorMessage](QString infoMessage) {
		main_bar_->session_error(errorMessage, infoMessage); });

	set_name(QFileInfo(file_name).fileName());
}

Session::capture_state Session::get_capture_state() const
{
	lock_guard<mutex> lock(sampling_mutex_);
	return capture_state_;
}

void Session::start_capture(function<void (const QString)> error_handler)
{
	if (!device_) {
		error_handler(tr("No active device set, can't start acquisition."));
		return;
	}

	stop_capture();

	// Check that at least one channel is enabled
	const shared_ptr<sigrok::Device> sr_dev = device_->device();
	if (sr_dev) {
		const auto channels = sr_dev->channels();
		if (!std::any_of(channels.begin(), channels.end(),
			[](shared_ptr<Channel> channel) {
				return channel->enabled(); })) {
			error_handler(tr("No channels enabled."));
			return;
		}
	}

	// Clear signal data
	for (const shared_ptr<data::SignalData> d : all_signal_data_)
		d->clear();

	// Revert name back to default name (e.g. "Session 1") as the data is gone
	name_ = default_name_;
	name_changed();

	// Begin the session
	sampling_thread_ = std::thread(
		&Session::sample_thread_proc, this, error_handler);
}

void Session::stop_capture()
{
	if (get_capture_state() != Stopped)
		device_->stop();

	// Check that sampling stopped
	if (sampling_thread_.joinable())
		sampling_thread_.join();
}

void Session::register_view(std::shared_ptr<views::ViewBase> view)
{
	if (views_.empty()) {
		main_view_ = view;
	}

	views_.push_back(view);

	update_signals();
}

void Session::deregister_view(std::shared_ptr<views::ViewBase> view)
{
	views_.remove_if([&](std::shared_ptr<views::ViewBase> v) {
		return v == view; });

	if (views_.empty()) {
		main_view_.reset();

		// Without a view there can be no main bar
		main_bar_.reset();
	}
}

bool Session::has_view(std::shared_ptr<views::ViewBase> view)
{
	for (std::shared_ptr<views::ViewBase> v : views_)
		if (v == view)
			return true;

	return false;
}

double Session::get_samplerate() const
{
	double samplerate = 0.0;

	for (const shared_ptr<pv::data::SignalData> d : all_signal_data_) {
		assert(d);
		const vector< shared_ptr<pv::data::Segment> > segments =
			d->segments();
		for (const shared_ptr<pv::data::Segment> &s : segments)
			samplerate = std::max(samplerate, s->samplerate());
	}
	// If there is no sample rate given we use samples as unit
	if (samplerate == 0.0)
		samplerate = 1.0;

	return samplerate;
}

const std::unordered_set< std::shared_ptr<data::SignalBase> >
	Session::signalbases() const
{
	return signalbases_;
}

#ifdef ENABLE_DECODE
bool Session::add_decoder(srd_decoder *const dec)
{
	map<const srd_channel*, shared_ptr<data::SignalBase> > channels;
	shared_ptr<data::DecoderStack> decoder_stack;

	try {
		// Create the decoder
		decoder_stack = make_shared<data::DecoderStack>(*this, dec);

		// Make a list of all the channels
		std::vector<const srd_channel*> all_channels;
		for (const GSList *i = dec->channels; i; i = i->next)
			all_channels.push_back((const srd_channel*)i->data);
		for (const GSList *i = dec->opt_channels; i; i = i->next)
			all_channels.push_back((const srd_channel*)i->data);

		// Auto select the initial channels
		for (const srd_channel *pdch : all_channels)
			for (shared_ptr<data::SignalBase> b : signalbases_) {
				if (b->type() == ChannelType::LOGIC) {
					if (QString::fromUtf8(pdch->name).toLower().
						contains(b->name().toLower()))
						channels[pdch] = b;
				}
			}

		assert(decoder_stack);
		assert(!decoder_stack->stack().empty());
		assert(decoder_stack->stack().front());
		decoder_stack->stack().front()->set_channels(channels);

		// Create the decode signal
		shared_ptr<data::SignalBase> signalbase =
			make_shared<data::SignalBase>(nullptr);

		signalbase->set_decoder_stack(decoder_stack);
		signalbases_.insert(signalbase);

		for (std::shared_ptr<views::ViewBase> view : views_)
			view->add_decode_signal(signalbase);
	} catch (std::runtime_error e) {
		return false;
	}

	signals_changed();

	// Do an initial decode
	decoder_stack->begin_decode();

	return true;
}

void Session::remove_decode_signal(shared_ptr<data::SignalBase> signalbase)
{
	signalbases_.erase(signalbase);

	for (std::shared_ptr<views::ViewBase> view : views_)
		view->remove_decode_signal(signalbase);

	signals_changed();
}
#endif

void Session::set_capture_state(capture_state state)
{
	bool changed;

	{
		lock_guard<mutex> lock(sampling_mutex_);
		changed = capture_state_ != state;
		capture_state_ = state;
	}

	if (changed)
		capture_state_changed(state);
}

void Session::update_signals()
{
	if (!device_) {
		signalbases_.clear();
		logic_data_.reset();
		for (std::shared_ptr<views::ViewBase> view : views_) {
			view->clear_signals();
#ifdef ENABLE_DECODE
			view->clear_decode_signals();
#endif
		}
		return;
	}

	lock_guard<recursive_mutex> lock(data_mutex_);

	const shared_ptr<sigrok::Device> sr_dev = device_->device();
	if (!sr_dev) {
		signalbases_.clear();
		logic_data_.reset();
		for (std::shared_ptr<views::ViewBase> view : views_) {
			view->clear_signals();
#ifdef ENABLE_DECODE
			view->clear_decode_signals();
#endif
		}
		return;
	}

	// Detect what data types we will receive
	auto channels = sr_dev->channels();
	unsigned int logic_channel_count = std::count_if(
		channels.begin(), channels.end(),
		[] (shared_ptr<Channel> channel) {
			return channel->type() == ChannelType::LOGIC; });

	// Create data containers for the logic data segments
	{
		lock_guard<recursive_mutex> data_lock(data_mutex_);

		if (logic_channel_count == 0) {
			logic_data_.reset();
		} else if (!logic_data_ ||
			logic_data_->num_channels() != logic_channel_count) {
			logic_data_.reset(new data::Logic(
				logic_channel_count));
			assert(logic_data_);
		}
	}

	// Make the signals list
	for (std::shared_ptr<views::ViewBase> viewbase : views_) {
		views::TraceView::View *trace_view =
			qobject_cast<views::TraceView::View*>(viewbase.get());

		if (trace_view) {
			unordered_set< shared_ptr<views::TraceView::Signal> >
				prev_sigs(trace_view->signals());
			trace_view->clear_signals();

			for (auto channel : sr_dev->channels()) {
				shared_ptr<data::SignalBase> signalbase;
				shared_ptr<views::TraceView::Signal> signal;

				// Find the channel in the old signals
				const auto iter = std::find_if(
					prev_sigs.cbegin(), prev_sigs.cend(),
					[&](const shared_ptr<views::TraceView::Signal> &s) {
						return s->base()->channel() == channel;
					});
				if (iter != prev_sigs.end()) {
					// Copy the signal from the old set to the new
					signal = *iter;
					trace_view->add_signal(signal);
				} else {
					// Find the signalbase for this channel if possible
					signalbase.reset();
					for (const shared_ptr<data::SignalBase> b : signalbases_)
						if (b->channel() == channel)
							signalbase = b;

					switch(channel->type()->id()) {
					case SR_CHANNEL_LOGIC:
						if (!signalbase) {
							signalbase = make_shared<data::SignalBase>(channel);
							signalbases_.insert(signalbase);

							all_signal_data_.insert(logic_data_);
							signalbase->set_data(logic_data_);
						}

						signal = shared_ptr<views::TraceView::Signal>(
							new views::TraceView::LogicSignal(*this,
								device_, signalbase));
						trace_view->add_signal(signal);
						break;

					case SR_CHANNEL_ANALOG:
					{
						if (!signalbase) {
							signalbase = make_shared<data::SignalBase>(channel);
							signalbases_.insert(signalbase);

							shared_ptr<data::Analog> data(new data::Analog());
							all_signal_data_.insert(data);
							signalbase->set_data(data);
						}

						signal = shared_ptr<views::TraceView::Signal>(
							new views::TraceView::AnalogSignal(
								*this, signalbase));
						trace_view->add_signal(signal);
						break;
					}

					default:
						assert(0);
						break;
					}
				}
			}
		}
	}

	signals_changed();
}

shared_ptr<data::SignalBase> Session::signalbase_from_channel(
	shared_ptr<sigrok::Channel> channel) const
{
	for (shared_ptr<data::SignalBase> sig : signalbases_) {
		assert(sig);
		if (sig->channel() == channel)
			return sig;
	}
	return shared_ptr<data::SignalBase>();
}

void Session::sample_thread_proc(function<void (const QString)> error_handler)
{
	assert(error_handler);

	if (!device_)
		return;

	cur_samplerate_ = device_->read_config<uint64_t>(ConfigKey::SAMPLERATE);

	out_of_memory_ = false;

	try {
		device_->start();
	} catch (Error e) {
		error_handler(e.what());
		return;
	}

	set_capture_state(device_->session()->trigger() ?
		AwaitingTrigger : Running);

	device_->run();
	set_capture_state(Stopped);

	// Confirm that SR_DF_END was received
	if (cur_logic_segment_) {
		qDebug("SR_DF_END was not received.");
		assert(0);
	}

	// Optimize memory usage
	free_unused_memory();

	// We now have unsaved data unless we just "captured" from a file
	shared_ptr<devices::File> file_device =
		dynamic_pointer_cast<devices::File>(device_);

	if (!file_device)
		data_saved_ = false;

	if (out_of_memory_)
		error_handler(tr("Out of memory, acquisition stopped."));
}

void Session::free_unused_memory()
{
	for (shared_ptr<data::SignalData> data : all_signal_data_) {
		const vector< shared_ptr<data::Segment> > segments = data->segments();

		for (shared_ptr<data::Segment> segment : segments) {
			segment->free_unused_memory();
		}
	}
}

void Session::feed_in_header()
{
	cur_samplerate_ = device_->read_config<uint64_t>(ConfigKey::SAMPLERATE);
}

void Session::feed_in_meta(shared_ptr<Meta> meta)
{
	for (auto entry : meta->config()) {
		switch (entry.first->id()) {
		case SR_CONF_SAMPLERATE:
			// We can't rely on the header to always contain the sample rate,
			// so in case it's supplied via a meta packet, we use it.
			if (!cur_samplerate_)
				cur_samplerate_ = g_variant_get_uint64(entry.second.gobj());

			/// @todo handle samplerate changes
			break;
		default:
			// Unknown metadata is not an error.
			break;
		}
	}

	signals_changed();
}

void Session::feed_in_trigger()
{
	// The channel containing most samples should be most accurate
	uint64_t sample_count = 0;

	{
		for (const shared_ptr<pv::data::SignalData> d : all_signal_data_) {
			assert(d);
			uint64_t temp_count = 0;

			const vector< shared_ptr<pv::data::Segment> > segments =
				d->segments();
			for (const shared_ptr<pv::data::Segment> &s : segments)
				temp_count += s->get_sample_count();

			if (temp_count > sample_count)
				sample_count = temp_count;
		}
	}

	trigger_event(sample_count / get_samplerate());
}

void Session::feed_in_frame_begin()
{
	if (cur_logic_segment_ || !cur_analog_segments_.empty())
		frame_began();
}

void Session::feed_in_logic(shared_ptr<Logic> logic)
{
	lock_guard<recursive_mutex> lock(data_mutex_);

	if (!logic_data_) {
		// The only reason logic_data_ would not have been created is
		// if it was not possible to determine the signals when the
		// device was created.
		update_signals();
	}

	if (!cur_logic_segment_) {
		// This could be the first packet after a trigger
		set_capture_state(Running);

		// Create a new data segment
		cur_logic_segment_ = make_shared<data::LogicSegment>(
			*logic_data_, logic, cur_samplerate_);
		logic_data_->push_segment(cur_logic_segment_);

		// @todo Putting this here means that only listeners querying
		// for logic will be notified. Currently the only user of
		// frame_began is DecoderStack, but in future we need to signal
		// this after both analog and logic sweeps have begun.
		frame_began();
	} else {
		// Append to the existing data segment
		cur_logic_segment_->append_payload(logic);
	}

	data_received();
}

void Session::feed_in_analog(shared_ptr<Analog> analog)
{
	lock_guard<recursive_mutex> lock(data_mutex_);

	const vector<shared_ptr<Channel>> channels = analog->channels();
	const unsigned int channel_count = channels.size();
	const size_t sample_count = analog->num_samples() / channel_count;
	const float *data = static_cast<const float *>(analog->data_pointer());
	bool sweep_beginning = false;

	if (signalbases_.empty())
		update_signals();

	for (auto channel : channels) {
		shared_ptr<data::AnalogSegment> segment;

		// Try to get the segment of the channel
		const map< shared_ptr<Channel>, shared_ptr<data::AnalogSegment> >::
			iterator iter = cur_analog_segments_.find(channel);
		if (iter != cur_analog_segments_.end())
			segment = (*iter).second;
		else {
			// If no segment was found, this means we haven't
			// created one yet. i.e. this is the first packet
			// in the sweep containing this segment.
			sweep_beginning = true;

			// Find the analog data associated with the channel
			shared_ptr<data::SignalBase> base = signalbase_from_channel(channel);
			assert(base);

			shared_ptr<data::Analog> data(base->analog_data());
			assert(data);

			// Create a segment, keep it in the maps of channels
			segment = make_shared<data::AnalogSegment>(
				*data, cur_samplerate_);
			cur_analog_segments_[channel] = segment;

			// Push the segment into the analog data.
			data->push_segment(segment);
		}

		assert(segment);

		// Append the samples in the segment
		segment->append_interleaved_samples(data++, sample_count,
			channel_count);
	}

	if (sweep_beginning) {
		// This could be the first packet after a trigger
		set_capture_state(Running);
	}

	data_received();
}

void Session::data_feed_in(shared_ptr<sigrok::Device> device,
	shared_ptr<Packet> packet)
{
	static bool frame_began=false;

	(void)device;

	assert(device);
	assert(device == device_->device());
	assert(packet);

	switch (packet->type()->id()) {
	case SR_DF_HEADER:
		feed_in_header();
		break;

	case SR_DF_META:
		feed_in_meta(dynamic_pointer_cast<Meta>(packet->payload()));
		break;

	case SR_DF_TRIGGER:
		feed_in_trigger();
		break;

	case SR_DF_FRAME_BEGIN:
		feed_in_frame_begin();
		frame_began = true;
		break;

	case SR_DF_LOGIC:
		try {
			feed_in_logic(dynamic_pointer_cast<Logic>(packet->payload()));
		} catch (std::bad_alloc) {
			out_of_memory_ = true;
			device_->stop();
		}
		break;

	case SR_DF_ANALOG:
		try {
			feed_in_analog(dynamic_pointer_cast<Analog>(packet->payload()));
		} catch (std::bad_alloc) {
			out_of_memory_ = true;
			device_->stop();
		}
		break;

	case SR_DF_FRAME_END:
	case SR_DF_END:
	{
		{
			lock_guard<recursive_mutex> lock(data_mutex_);
			cur_logic_segment_.reset();
			cur_analog_segments_.clear();
		}
		if (frame_began) {
			frame_began = false;
			frame_ended();
		}
		break;
	}
	default:
		break;
	}
}

void Session::on_data_saved()
{
	data_saved_ = true;
}

} // namespace pv
