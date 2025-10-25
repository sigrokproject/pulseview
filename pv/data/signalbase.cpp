/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 Soeren Apel <soeren@apelpie.net>
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

#include "analog.hpp"
#include "analogsegment.hpp"
#include "decode/row.hpp"
#include "logic.hpp"
#include "logicsegment.hpp"
#include "signalbase.hpp"
#include "signaldata.hpp"

#include <QDebug>
#include <QUuid>

#include <extdef.h>
#include <pv/session.hpp>
#include <pv/binding/decoder.hpp>

using std::dynamic_pointer_cast;
using std::make_shared;
using std::out_of_range;
using std::shared_ptr;
using std::tie;
using std::unique_lock;

namespace pv {
namespace data {

const QColor SignalBase::AnalogSignalColors[8] =
{
	QColor(0xC4, 0xA0, 0x00),	// Yellow   HSV:  49 / 100 / 77
	QColor(0x87, 0x20, 0x7A),	// Magenta  HSV: 308 /  70 / 53
	QColor(0x20, 0x4A, 0x87),	// Blue     HSV: 216 /  76 / 53
	QColor(0x4E, 0x9A, 0x06),	// Green    HSV:  91 /  96 / 60
	QColor(0xBF, 0x6E, 0x00),	// Orange   HSV:  35 / 100 / 75
	QColor(0x5E, 0x20, 0x80),	// Purple   HSV: 280 /  75 / 50
	QColor(0x20, 0x80, 0x7A),	// Turqoise HSV: 177 /  75 / 50
	QColor(0x80, 0x20, 0x24)	// Red      HSV: 358 /  75 / 50
};

const QColor SignalBase::LogicSignalColors[10] =
{
	QColor(0x16, 0x19, 0x1A),	// Black
	QColor(0x8F, 0x52, 0x02),	// Brown
	QColor(0xCC, 0x00, 0x00),	// Red
	QColor(0xF5, 0x79, 0x00),	// Orange
	QColor(0xED, 0xD4, 0x00),	// Yellow
	QColor(0x73, 0xD2, 0x16),	// Green
	QColor(0x34, 0x65, 0xA4),	// Blue
	QColor(0x75, 0x50, 0x7B),	// Violet
	QColor(0x88, 0x8A, 0x85),	// Grey
	QColor(0xEE, 0xEE, 0xEC),	// White
};


const int SignalBase::ColorBGAlpha = 8 * 256 / 100;
const uint64_t SignalBase::ConversionBlockSize = 4096;
const uint32_t SignalBase::ConversionDelay = 1000;  // 1 second


SignalGroup::SignalGroup(const QString& name)
{
	name_ = name;
}

void SignalGroup::append_signal(shared_ptr<SignalBase> signal)
{
	if (!signal)
		return;

	signals_.push_back(signal);
	signal->set_group(this);
}

void SignalGroup::remove_signal(shared_ptr<SignalBase> signal)
{
	if (!signal)
		return;

	signals_.erase(std::remove_if(signals_.begin(), signals_.end(),
		[&](shared_ptr<SignalBase> s) { return s == signal; }),
		signals_.end());
}

deque<shared_ptr<SignalBase>> SignalGroup::signals() const
{
	return signals_;
}

void SignalGroup::clear()
{
	for (shared_ptr<SignalBase> sb : signals_)
		sb->set_group(nullptr);

	signals_.clear();
}

const QString SignalGroup::name() const
{
	return name_;
}


SignalBase::SignalBase(shared_ptr<sigrok::Channel> channel, ChannelType channel_type) :
	channel_(channel),
	channel_type_(channel_type),
	group_(nullptr),
	conversion_type_(NoConversion),
	min_value_(0),
	max_value_(0),
	index_(0),
	error_message_("")
{
	if (channel_) {
		set_internal_name(QString::fromStdString(channel_->name()));
		set_index(channel_->index());
	} else {
		set_internal_name(QUuid::createUuid().toString().remove('{').remove('}'));
	}

	connect(&delayed_conversion_starter_, SIGNAL(timeout()),
		this, SLOT(on_delayed_conversion_start()));
	delayed_conversion_starter_.setSingleShot(true);
	delayed_conversion_starter_.setInterval(ConversionDelay);

	// Only logic and analog SR channels can have their colors auto-set
	// because for them, we have an index that can be used
	if (channel_type == LogicChannel)
		set_color(LogicSignalColors[index() % countof(LogicSignalColors)]);
	else if (channel_type == AnalogChannel)
		set_color(AnalogSignalColors[index() % countof(AnalogSignalColors)]);
}

SignalBase::~SignalBase()
{
	stop_conversion();
}

shared_ptr<sigrok::Channel> SignalBase::channel() const
{
	return channel_;
}

bool SignalBase::is_generated() const
{
	// Only signals associated with a device have a corresponding sigrok channel
	return channel_ == nullptr;
}

bool SignalBase::enabled() const
{
	return (channel_) ? channel_->enabled() : true;
}

void SignalBase::set_enabled(bool value)
{
	if (channel_) {
		channel_->set_enabled(value);
		enabled_changed(value);
	}
}

SignalBase::ChannelType SignalBase::type() const
{
	return channel_type_;
}

unsigned int SignalBase::index() const
{
	return index_;
}

void SignalBase::set_index(unsigned int index)
{
	index_ = index;
}

unsigned int SignalBase::logic_bit_index() const
{
	if (channel_type_ == LogicChannel)
		return index_;
	else
		return 0;
}

void SignalBase::set_group(SignalGroup* group)
{
	group_ = group;
}

SignalGroup* SignalBase::group() const
{
	return group_;
}

QString SignalBase::name() const
{
	return (channel_) ? QString::fromStdString(channel_->name()) : name_;
}

QString SignalBase::internal_name() const
{
	return internal_name_;
}

void SignalBase::set_internal_name(QString internal_name)
{
	internal_name_ = internal_name;

	// Use this name also for the QObject instance
	setObjectName(internal_name);
}

QString SignalBase::display_name() const
{
	if ((name() != internal_name_) && (!internal_name_.isEmpty()))
		return name() + " (" + internal_name_ + ")";
	else
		return name();
}

void SignalBase::set_name(QString name)
{
	if (channel_)
		channel_->set_name(name.toUtf8().constData());

	name_ = name;

	name_changed(name);
}

QColor SignalBase::color() const
{
	return color_;
}

void SignalBase::set_color(QColor color)
{
	color_ = color;

	bgcolor_ = color;
	bgcolor_.setAlpha(ColorBGAlpha);

	color_changed(color);
}

QColor SignalBase::bgcolor() const
{
	return bgcolor_;
}

QString SignalBase::get_error_message() const
{
	return error_message_;
}

void SignalBase::set_data(shared_ptr<pv::data::SignalData> data)
{
	if (data_) {
		disconnect(data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		disconnect(data.get(), SIGNAL(samples_added(shared_ptr<Segment>, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(shared_ptr<Segment>, uint64_t, uint64_t)));

		shared_ptr<Analog> analog = analog_data();
		if (analog)
			disconnect(analog.get(), SIGNAL(min_max_changed(float, float)),
				this, SLOT(on_min_max_changed(float, float)));
	}

	data_ = data;

	if (data_) {
		connect(data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		connect(data.get(), SIGNAL(samples_added(SharedPtrToSegment, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(SharedPtrToSegment, uint64_t, uint64_t)));

		shared_ptr<Analog> analog = analog_data();
		if (analog)
			connect(analog.get(), SIGNAL(min_max_changed(float, float)),
				this, SLOT(on_min_max_changed(float, float)));
	}
}

void SignalBase::clear_sample_data()
{
	if (analog_data())
		analog_data()->clear();

	if (logic_data())
		logic_data()->clear();
}

shared_ptr<data::Analog> SignalBase::analog_data() const
{
	if (!data_)
		return nullptr;

	return dynamic_pointer_cast<Analog>(data_);
}

shared_ptr<data::Logic> SignalBase::logic_data() const
{
	if (!data_)
		return nullptr;

	shared_ptr<Logic> result;

	if (((conversion_type_ == A2LConversionByThreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger)))
		result = dynamic_pointer_cast<Logic>(converted_data_);
	else
		result = dynamic_pointer_cast<Logic>(data_);

	return result;
}

shared_ptr<pv::data::SignalData> SignalBase::data() const
{
	return data_;
}

bool SignalBase::segment_is_complete(uint32_t segment_id) const
{
	bool result = true;

	shared_ptr<Analog> adata = analog_data();
	if (adata)
	{
		auto segments = adata->analog_segments();
		try {
			result = segments.at(segment_id)->is_complete();
		} catch (out_of_range&) {
			// Do nothing
		}
	} else {
		shared_ptr<Logic> ldata = logic_data();
		if (ldata) {
			shared_ptr<Logic> data = dynamic_pointer_cast<Logic>(data_);
			auto segments = data->logic_segments();
			try {
				result = segments.at(segment_id)->is_complete();
			} catch (out_of_range&) {
				// Do nothing
			}
		}
	}

	return result;
}

bool SignalBase::has_samples() const
{
	bool result = false;

	shared_ptr<Analog> adata = analog_data();
	if (adata)
	{
		auto segments = adata->analog_segments();
		if ((segments.size() > 0) && (segments.front()->get_sample_count() > 0))
			result = true;
	} else {
		shared_ptr<Logic> ldata = logic_data();
		if (ldata) {
			auto segments = ldata->logic_segments();
			if ((segments.size() > 0) && (segments.front()->get_sample_count() > 0))
				result = true;
		}
	}

	return result;
}

double SignalBase::get_samplerate() const
{
	shared_ptr<Analog> adata = analog_data();
	if (adata)
		return adata->get_samplerate();
	else {
		shared_ptr<Logic> ldata = logic_data();
		if (ldata)
			return ldata->get_samplerate();
	}

	// Default samplerate is 1 Hz
	return 1.0;
}

SignalBase::ConversionType SignalBase::get_conversion_type() const
{
	return conversion_type_;
}

void SignalBase::set_conversion_type(ConversionType t)
{
	if (conversion_type_ != NoConversion) {
		stop_conversion();

		// Discard converted data
		converted_data_.reset();
		samples_cleared();
	}

	conversion_type_ = t;

	// Re-create an empty container
	// so that the signal is recognized as providing logic data
	// and thus can be assigned to a decoder
	if (conversion_is_a2l())
		if (!converted_data_)
			converted_data_ = make_shared<Logic>(1);  // Contains only one channel

	start_conversion();

	conversion_type_changed(t);
}

map<QString, QVariant> SignalBase::get_conversion_options() const
{
	return conversion_options_;
}

bool SignalBase::set_conversion_option(QString key, QVariant value)
{
	QVariant old_value;

	auto key_iter = conversion_options_.find(key);
	if (key_iter != conversion_options_.end())
		old_value = key_iter->second;

	conversion_options_[key] = value;

	return (value != old_value);
}

vector<double> SignalBase::get_conversion_thresholds(const ConversionType t,
	const bool always_custom) const
{
	vector<double> result;
	ConversionType conv_type = t;
	ConversionPreset preset;

	// Use currently active conversion if no conversion type was supplied
	if (conv_type == NoConversion)
		conv_type = conversion_type_;

	if (always_custom)
		preset = NoPreset;
	else
		preset = get_current_conversion_preset();

	if (conv_type == A2LConversionByThreshold) {
		double thr = 0;

		if (preset == NoPreset) {
			auto thr_iter = conversion_options_.find("threshold_value");
			if (thr_iter != conversion_options_.end())
				thr = (thr_iter->second).toDouble();
		}

		if (preset == DynamicPreset)
			thr = (min_value_ + max_value_) * 0.5;  // middle between min and max

		if ((int)preset == 1) thr = 0.9;
		if ((int)preset == 2) thr = 1.8;
		if ((int)preset == 3) thr = 2.5;
		if ((int)preset == 4) thr = 1.5;

		result.push_back(thr);
	}

	if (conv_type == A2LConversionBySchmittTrigger) {
		double thr_lo = 0, thr_hi = 0;

		if (preset == NoPreset) {
			auto thr_lo_iter = conversion_options_.find("threshold_value_low");
			if (thr_lo_iter != conversion_options_.end())
				thr_lo = (thr_lo_iter->second).toDouble();

			auto thr_hi_iter = conversion_options_.find("threshold_value_high");
			if (thr_hi_iter != conversion_options_.end())
				thr_hi = (thr_hi_iter->second).toDouble();
		}

		if (preset == DynamicPreset) {
			const double amplitude = max_value_ - min_value_;
			const double center = min_value_ + (amplitude / 2);
			thr_lo = center - (amplitude * 0.15);  // 15% margin
			thr_hi = center + (amplitude * 0.15);  // 15% margin
		}

		if ((int)preset == 1) { thr_lo = 0.3; thr_hi = 1.2; }
		if ((int)preset == 2) { thr_lo = 0.7; thr_hi = 2.5; }
		if ((int)preset == 3) { thr_lo = 1.3; thr_hi = 3.7; }
		if ((int)preset == 4) { thr_lo = 0.8; thr_hi = 2.0; }

		result.push_back(thr_lo);
		result.push_back(thr_hi);
	}

	return result;
}

vector< pair<QString, int> > SignalBase::get_conversion_presets() const
{
	vector< pair<QString, int> > presets;

	if (conversion_type_ == A2LConversionByThreshold) {
		// Source: http://www.interfacebus.com/voltage_threshold.html
		presets.emplace_back(tr("Signal average"), 0);
		presets.emplace_back(tr("0.9V (for 1.8V CMOS)"), 1);
		presets.emplace_back(tr("1.8V (for 3.3V CMOS)"), 2);
		presets.emplace_back(tr("2.5V (for 5.0V CMOS)"), 3);
		presets.emplace_back(tr("1.5V (for TTL)"), 4);
	}

	if (conversion_type_ == A2LConversionBySchmittTrigger) {
		// Source: http://www.interfacebus.com/voltage_threshold.html
		presets.emplace_back(tr("Signal average +/- 15%"), 0);
		presets.emplace_back(tr("0.3V/1.2V (for 1.8V CMOS)"), 1);
		presets.emplace_back(tr("0.7V/2.5V (for 3.3V CMOS)"), 2);
		presets.emplace_back(tr("1.3V/3.7V (for 5.0V CMOS)"), 3);
		presets.emplace_back(tr("0.8V/2.0V (for TTL)"), 4);
	}

	return presets;
}

SignalBase::ConversionPreset SignalBase::get_current_conversion_preset() const
{
	auto preset = conversion_options_.find("preset");
	if (preset != conversion_options_.end())
		return (ConversionPreset)((preset->second).toInt());

	return DynamicPreset;
}

void SignalBase::set_conversion_preset(ConversionPreset id)
{
	conversion_options_["preset"] = (int)id;
}

#ifdef ENABLE_DECODE
bool SignalBase::is_decode_signal() const
{
	return (channel_type_ == DecodeChannel);
}
#endif

void SignalBase::save_settings(QSettings &settings) const
{
	settings.setValue("name", name());
	if (is_generated())
		settings.setValue("uuid", internal_name());
	settings.setValue("enabled", enabled());
	settings.setValue("color", color().rgba());
	settings.setValue("conversion_type", (int)conversion_type_);

	settings.setValue("conv_options", (int)(conversion_options_.size()));
	int i = 0;
	for (auto& kvp : conversion_options_) {
		settings.setValue(QString("conv_option%1_key").arg(i), kvp.first);
		settings.setValue(QString("conv_option%1_value").arg(i), kvp.second);
		i++;
	}
}

void SignalBase::restore_settings(QSettings &settings)
{
	if (settings.contains("name"))
		set_name(settings.value("name").toString());

	if (is_generated() && settings.contains("uuid"))
		set_internal_name(settings.value("uuid").toString());

	if (settings.contains("enabled"))
		set_enabled(settings.value("enabled").toBool());

	if (settings.contains("color")) {
		QVariant value = settings.value("color");

		// Workaround for Qt QColor serialization bug on OSX
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		bool is_qcolor = (QMetaType::Type)(value.typeId()) == QMetaType::QColor;
#else
		bool is_qcolor = (QMetaType::Type)(value.type()) == QMetaType::QColor;
#endif
		if (is_qcolor)
			set_color(value.value<QColor>());
		else
			set_color(QColor::fromRgba(value.value<uint32_t>()));

		// A color with an alpha value of 0 makes the signal marker invisible
		if (color() == QColor(0, 0, 0, 0))
			set_color(Qt::gray);
	}

	if (settings.contains("conversion_type"))
		set_conversion_type((ConversionType)settings.value("conversion_type").toInt());

	int conv_options = 0;
	if (settings.contains("conv_options"))
		conv_options = settings.value("conv_options").toInt();

	if (conv_options)
		for (int i = 0; i < conv_options; i++) {
			const QString key_id = QString("conv_option%1_key").arg(i);
			const QString value_id = QString("conv_option%1_value").arg(i);

			if (settings.contains(key_id) && settings.contains(value_id))
				conversion_options_[settings.value(key_id).toString()] =
					settings.value(value_id);
		}
}

bool SignalBase::conversion_is_a2l() const
{
	return (((conversion_type_ == A2LConversionByThreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger)));
}

void SignalBase::convert_single_segment_range(shared_ptr<AnalogSegment> asegment,
	shared_ptr<LogicSegment> lsegment, uint64_t start_sample, uint64_t end_sample)
{
	if (end_sample > start_sample) {
		tie(min_value_, max_value_) = asegment->get_min_max();

		// Create sigrok::Analog instance
		float *asamples = new float[ConversionBlockSize];
		assert(asamples);
		uint8_t *lsamples = new uint8_t[ConversionBlockSize];
		assert(lsamples);

		vector<shared_ptr<sigrok::Channel> > channels;
		if (channel_)
			channels.push_back(channel_);

		vector<const sigrok::QuantityFlag*> mq_flags;
		const sigrok::Quantity * const mq = sigrok::Quantity::VOLTAGE;
		const sigrok::Unit * const unit = sigrok::Unit::VOLT;

		shared_ptr<sigrok::Packet> packet =
			Session::sr_context->create_analog_packet(channels,
			asamples, ConversionBlockSize, mq, unit, mq_flags);

		shared_ptr<sigrok::Analog> analog =
			dynamic_pointer_cast<sigrok::Analog>(packet->payload());

		// Convert
		uint64_t i = start_sample;

		if (conversion_type_ == A2LConversionByThreshold) {
			const double threshold = get_conversion_thresholds()[0];

			// Convert as many sample blocks as we can
			while ((end_sample - i) > ConversionBlockSize) {
				asegment->get_samples(i, i + ConversionBlockSize, asamples);

				shared_ptr<sigrok::Logic> logic =
					analog->get_logic_via_threshold(threshold, lsamples);

				lsegment->append_payload(logic->data_pointer(), logic->data_length());
				samples_added(lsegment->segment_id(), i, i + ConversionBlockSize);
				i += ConversionBlockSize;
			}

			// Re-create sigrok::Analog and convert remaining samples
			packet = Session::sr_context->create_analog_packet(channels,
				asamples, end_sample - i, mq, unit, mq_flags);

			analog = dynamic_pointer_cast<sigrok::Analog>(packet->payload());

			asegment->get_samples(i, end_sample, asamples);
			shared_ptr<sigrok::Logic> logic =
				analog->get_logic_via_threshold(threshold, lsamples);
			lsegment->append_payload(logic->data_pointer(), logic->data_length());
			samples_added(lsegment->segment_id(), i, end_sample);
		}

		if (conversion_type_ == A2LConversionBySchmittTrigger) {
			const vector<double> thresholds = get_conversion_thresholds();
			const double lo_thr = thresholds[0];
			const double hi_thr = thresholds[1];

			uint8_t state = 0;  // TODO Use value of logic sample n-1 instead of 0

			// Convert as many sample blocks as we can
			while ((end_sample - i) > ConversionBlockSize) {
				asegment->get_samples(i, i + ConversionBlockSize, asamples);

				shared_ptr<sigrok::Logic> logic =
					analog->get_logic_via_schmitt_trigger(lo_thr, hi_thr,
						&state, lsamples);

				lsegment->append_payload(logic->data_pointer(), logic->data_length());
				samples_added(lsegment->segment_id(), i, i + ConversionBlockSize);

				i += ConversionBlockSize;
			}

			// Re-create sigrok::Analog and convert remaining samples
			packet = Session::sr_context->create_analog_packet(channels,
				asamples, end_sample - i, mq, unit, mq_flags);

			analog = dynamic_pointer_cast<sigrok::Analog>(packet->payload());

			asegment->get_samples(i, end_sample, asamples);
			shared_ptr<sigrok::Logic> logic =
				analog->get_logic_via_schmitt_trigger(lo_thr, hi_thr,
					&state, lsamples);
			lsegment->append_payload(logic->data_pointer(), logic->data_length());
			samples_added(lsegment->segment_id(), i, end_sample);
		}

		// If acquisition is ongoing, start-/endsample may have changed
		end_sample = asegment->get_sample_count();

		delete[] lsamples;
		delete[] asamples;
	}

	samples_added(lsegment->segment_id(), start_sample, end_sample);
}

void SignalBase::convert_single_segment(shared_ptr<AnalogSegment> asegment,
	shared_ptr<LogicSegment> lsegment)
{
	uint64_t start_sample, end_sample, old_end_sample;
	start_sample = end_sample = 0;
	bool complete_state, old_complete_state;

	start_sample = lsegment->get_sample_count();
	end_sample = asegment->get_sample_count();
	complete_state = asegment->is_complete();

	// Don't do anything if the segment is still being filled and the sample count is too small
	if ((!complete_state) && (end_sample - start_sample < ConversionBlockSize))
		return;

	do {
		convert_single_segment_range(asegment, lsegment, start_sample, end_sample);

		old_end_sample = end_sample;
		old_complete_state = complete_state;

		start_sample = lsegment->get_sample_count();
		end_sample = asegment->get_sample_count();
		complete_state = asegment->is_complete();

		// If the segment has been incomplete when we were called and has been
		// completed in the meanwhile, we convert the remaining samples as well.
		// Also, if a sufficient number of samples was added in the meanwhile,
		// we do another round of sample conversion.
	} while ((complete_state != old_complete_state) ||
		(end_sample - old_end_sample >= ConversionBlockSize));

	if (complete_state)
		lsegment->set_complete();
}

void SignalBase::conversion_thread_proc()
{
	shared_ptr<Analog> analog_data;

	if (conversion_is_a2l()) {
		analog_data = dynamic_pointer_cast<Analog>(data_);

		if (analog_data->analog_segments().size() == 0) {
			unique_lock<mutex> input_lock(conversion_input_mutex_);
			conversion_input_cond_.wait(input_lock);
		}

	} else
		// Currently, we only handle A2L conversions
		return;

	// If we had to wait for input data, we may have been notified to terminate
	if (conversion_interrupt_)
		return;

	uint32_t segment_id = 0;

	shared_ptr<AnalogSegment> asegment = analog_data->analog_segments().front();
	assert(asegment);
	connect(asegment.get(), SIGNAL(completed()), this, SLOT(on_input_segment_completed()));

	const shared_ptr<Logic> logic_data = dynamic_pointer_cast<Logic>(converted_data_);
	assert(logic_data);

	// Create the initial logic data segment if needed
	if (logic_data->logic_segments().size() == 0) {
		shared_ptr<LogicSegment> new_segment =
			make_shared<LogicSegment>(*logic_data.get(), 0, 1, asegment->samplerate());
		logic_data->push_segment(new_segment);
	}

	shared_ptr<LogicSegment> lsegment = logic_data->logic_segments().front();
	assert(lsegment);

	do {
		convert_single_segment(asegment, lsegment);

		// Only advance to next segment if the current input segment is complete
		if (asegment->is_complete() &&
			analog_data->analog_segments().size() > logic_data->logic_segments().size()) {

			disconnect(asegment.get(), SIGNAL(completed()), this, SLOT(on_input_segment_completed()));

			// There are more segments to process
			segment_id++;

			try {
				asegment = analog_data->analog_segments().at(segment_id);
				disconnect(asegment.get(), SIGNAL(completed()), this, SLOT(on_input_segment_completed()));
				connect(asegment.get(), SIGNAL(completed()), this, SLOT(on_input_segment_completed()));
			} catch (out_of_range&) {
				qDebug() << "Conversion error for" << name() << ": no analog segment" \
					<< segment_id << ", segments size is" << analog_data->analog_segments().size();
				return;
			}

			shared_ptr<LogicSegment> new_segment = make_shared<LogicSegment>(
				*logic_data.get(), segment_id, 1, asegment->samplerate());
			logic_data->push_segment(new_segment);

			lsegment = logic_data->logic_segments().back();
		} else {
			// No more samples/segments to process, wait for data or interrupt
			if (!conversion_interrupt_) {
				unique_lock<mutex> input_lock(conversion_input_mutex_);
				conversion_input_cond_.wait(input_lock);
			}
		}
	} while (!conversion_interrupt_);

	disconnect(asegment.get(), SIGNAL(completed()), this, SLOT(on_input_segment_completed()));
}

void SignalBase::start_conversion(bool delayed_start)
{
	if (delayed_start) {
		delayed_conversion_starter_.start();
		return;
	}

	stop_conversion();

	if (converted_data_ && (converted_data_->get_segment_count() > 0)) {
		converted_data_->clear();
		samples_cleared();
	}

	conversion_interrupt_ = false;
	conversion_thread_ = std::thread(&SignalBase::conversion_thread_proc, this);
}

void SignalBase::set_error_message(QString msg)
{
	error_message_ = msg;
	// TODO Emulate noquote()
	qDebug().nospace() << name() << ": " << msg;

	error_message_changed(msg);
}

void SignalBase::stop_conversion()
{
	// Stop conversion so we can restart it from the beginning
	conversion_interrupt_ = true;
	conversion_input_cond_.notify_one();
	if (conversion_thread_.joinable())
		conversion_thread_.join();
}

void SignalBase::on_samples_cleared()
{
	if (converted_data_ && (converted_data_->get_segment_count() > 0)) {
		converted_data_->clear();
		samples_cleared();
	}
}

void SignalBase::on_samples_added(SharedPtrToSegment segment, uint64_t start_sample,
	uint64_t end_sample)
{
	if (conversion_type_ != NoConversion) {
		if (conversion_thread_.joinable()) {
			// Notify the conversion thread since it's running
			conversion_input_cond_.notify_one();
		} else {
			// Start the conversion thread unless the delay timer is running
			if (!delayed_conversion_starter_.isActive())
				start_conversion();
		}
	}

	samples_added(segment->segment_id(), start_sample, end_sample);
}

void SignalBase::on_input_segment_completed()
{
	if (conversion_type_ != NoConversion)
		if (conversion_thread_.joinable()) {
			// Notify the conversion thread since it's running
			conversion_input_cond_.notify_one();
		}
}

void SignalBase::on_min_max_changed(float min, float max)
{
	// Restart conversion if one is enabled and uses a calculated threshold
	if ((conversion_type_ != NoConversion) &&
		(get_current_conversion_preset() == DynamicPreset))
		start_conversion(true);

	min_max_changed(min, max);
}

void SignalBase::on_capture_state_changed(int state)
{
	if (state == Session::Running) {
		// Restart conversion if one is enabled
		if (conversion_type_ != NoConversion)
			start_conversion();
	}
}

void SignalBase::on_delayed_conversion_start()
{
	start_conversion();
}

} // namespace data
} // namespace pv
