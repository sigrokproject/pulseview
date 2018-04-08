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

#include <pv/binding/decoder.hpp>
#include <pv/session.hpp>

using std::dynamic_pointer_cast;
using std::make_shared;
using std::out_of_range;
using std::shared_ptr;
using std::tie;
using std::unique_lock;

namespace pv {
namespace data {

const int SignalBase::ColourBGAlpha = 8 * 256 / 100;
const uint64_t SignalBase::ConversionBlockSize = 4096;
const uint32_t SignalBase::ConversionDelay = 1000;  // 1 second

SignalBase::SignalBase(shared_ptr<sigrok::Channel> channel, ChannelType channel_type) :
	channel_(channel),
	channel_type_(channel_type),
	conversion_type_(NoConversion),
	min_value_(0),
	max_value_(0)
{
	if (channel_)
		internal_name_ = QString::fromStdString(channel_->name());

	connect(&delayed_conversion_starter_, SIGNAL(timeout()),
		this, SLOT(on_delayed_conversion_start()));
	delayed_conversion_starter_.setSingleShot(true);
	delayed_conversion_starter_.setInterval(ConversionDelay);
}

SignalBase::~SignalBase()
{
	stop_conversion();
}

shared_ptr<sigrok::Channel> SignalBase::channel() const
{
	return channel_;
}

QString SignalBase::name() const
{
	return (channel_) ? QString::fromStdString(channel_->name()) : name_;
}

QString SignalBase::internal_name() const
{
	return internal_name_;
}

QString SignalBase::display_name() const
{
	if (name() != internal_name_)
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
	return (channel_) ? channel_->index() : 0;
}

unsigned int SignalBase::logic_bit_index() const
{
	if (channel_type_ == LogicChannel)
		return channel_->index();
	else
		return 0;
}

QColor SignalBase::colour() const
{
	return colour_;
}

void SignalBase::set_colour(QColor colour)
{
	colour_ = colour;

	bgcolour_ = colour;
	bgcolour_.setAlpha(ColourBGAlpha);

	colour_changed(colour);
}

QColor SignalBase::bgcolour() const
{
	return bgcolour_;
}

void SignalBase::set_data(shared_ptr<pv::data::SignalData> data)
{
	if (data_) {
		disconnect(data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		disconnect(data.get(), SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(QObject*, uint64_t, uint64_t)));

		if (channel_type_ == AnalogChannel) {
			shared_ptr<Analog> analog = analog_data();
			assert(analog);

			disconnect(analog.get(), SIGNAL(min_max_changed(float, float)),
				this, SLOT(on_min_max_changed(float, float)));
		}
	}

	data_ = data;

	if (data_) {
		connect(data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		connect(data.get(), SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(QObject*, uint64_t, uint64_t)));

		if (channel_type_ == AnalogChannel) {
			shared_ptr<Analog> analog = analog_data();
			assert(analog);

			connect(analog.get(), SIGNAL(min_max_changed(float, float)),
				this, SLOT(on_min_max_changed(float, float)));
		}
	}
}

shared_ptr<data::Analog> SignalBase::analog_data() const
{
	shared_ptr<Analog> result = nullptr;

	if (channel_type_ == AnalogChannel)
		result = dynamic_pointer_cast<Analog>(data_);

	return result;
}

shared_ptr<data::Logic> SignalBase::logic_data() const
{
	shared_ptr<Logic> result = nullptr;

	if (channel_type_ == LogicChannel)
		result = dynamic_pointer_cast<Logic>(data_);

	if (((conversion_type_ == A2LConversionByThreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger)))
		result = dynamic_pointer_cast<Logic>(converted_data_);

	return result;
}

bool SignalBase::segment_is_complete(uint32_t segment_id) const
{
	bool result = true;

	if (channel_type_ == AnalogChannel)
	{
		shared_ptr<Analog> data = dynamic_pointer_cast<Analog>(data_);
		auto segments = data->analog_segments();
		try {
			result = segments.at(segment_id)->is_complete();
		} catch (out_of_range&) {
			// Do nothing
		}
	}

	if (channel_type_ == LogicChannel)
	{
		shared_ptr<Logic> data = dynamic_pointer_cast<Logic>(data_);
		auto segments = data->logic_segments();
		try {
			result = segments.at(segment_id)->is_complete();
		} catch (out_of_range&) {
			// Do nothing
		}
	}

	return result;
}

bool SignalBase::has_samples() const
{
	bool result = false;

	if (channel_type_ == AnalogChannel)
	{
		shared_ptr<Analog> data = dynamic_pointer_cast<Analog>(data_);
		if (data) {
			auto segments = data->analog_segments();
			if ((segments.size() > 0) && (segments.front()->get_sample_count() > 0))
				result = true;
		}
	}

	if (channel_type_ == LogicChannel)
	{
		shared_ptr<Logic> data = dynamic_pointer_cast<Logic>(data_);
		if (data) {
			auto segments = data->logic_segments();
			if ((segments.size() > 0) && (segments.front()->get_sample_count() > 0))
				result = true;
		}
	}

	return result;
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
	settings.setValue("enabled", enabled());
	settings.setValue("colour", colour());
	settings.setValue("conversion_type", (int)conversion_type_);

	settings.setValue("conv_options", (int)(conversion_options_.size()));
	int i = 0;
	for (auto kvp : conversion_options_) {
		settings.setValue(QString("conv_option%1_key").arg(i), kvp.first);
		settings.setValue(QString("conv_option%1_value").arg(i), kvp.second);
		i++;
	}
}

void SignalBase::restore_settings(QSettings &settings)
{
	set_name(settings.value("name").toString());
	set_enabled(settings.value("enabled").toBool());
	set_colour(settings.value("colour").value<QColor>());
	set_conversion_type((ConversionType)settings.value("conversion_type").toInt());

	int conv_options = settings.value("conv_options").toInt();

	if (conv_options)
		for (int i = 0; i < conv_options; i++) {
			QString key = settings.value(QString("conv_option%1_key").arg(i)).toString();
			QVariant value = settings.value(QString("conv_option%1_value").arg(i));
			conversion_options_[key] = value;
		}
}

bool SignalBase::conversion_is_a2l() const
{
	return ((channel_type_ == AnalogChannel) &&
		((conversion_type_ == A2LConversionByThreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger)));
}

void SignalBase::convert_single_segment_range(AnalogSegment *asegment,
	LogicSegment *lsegment, uint64_t start_sample, uint64_t end_sample)
{
	if (end_sample > start_sample) {
		tie(min_value_, max_value_) = asegment->get_min_max();

		// Create sigrok::Analog instance
		float *asamples = new float[ConversionBlockSize];
		uint8_t *lsamples = new uint8_t[ConversionBlockSize];

		vector<shared_ptr<sigrok::Channel> > channels;
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
}

void SignalBase::convert_single_segment(AnalogSegment *asegment, LogicSegment *lsegment)
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

	AnalogSegment *asegment = analog_data->analog_segments().front().get();
	assert(asegment);

	const shared_ptr<Logic> logic_data = dynamic_pointer_cast<Logic>(converted_data_);
	assert(logic_data);

	// Create the initial logic data segment if needed
	if (logic_data->logic_segments().size() == 0) {
		shared_ptr<LogicSegment> new_segment =
			make_shared<LogicSegment>(*logic_data.get(), 0, 1, asegment->samplerate());
		logic_data->push_segment(new_segment);
	}

	LogicSegment *lsegment = logic_data->logic_segments().front().get();
	assert(lsegment);

	do {
		convert_single_segment(asegment, lsegment);

		// Only advance to next segment if the current input segment is complete
		if (asegment->is_complete() &&
			analog_data->analog_segments().size() > logic_data->logic_segments().size()) {
			// There are more segments to process
			segment_id++;

			try {
				asegment = analog_data->analog_segments().at(segment_id).get();
			} catch (out_of_range&) {
				qDebug() << "Conversion error for" << name() << ": no analog segment" \
					<< segment_id << ", segments size is" << analog_data->analog_segments().size();
				return;
			}

			shared_ptr<LogicSegment> new_segment = make_shared<LogicSegment>(
				*logic_data.get(), segment_id, 1, asegment->samplerate());
			logic_data->push_segment(new_segment);

			lsegment = logic_data->logic_segments().back().get();
		} else {
			// No more samples/segments to process, wait for data or interrupt
			if (!conversion_interrupt_) {
				unique_lock<mutex> input_lock(conversion_input_mutex_);
				conversion_input_cond_.wait(input_lock);
			}
		}
	} while (!conversion_interrupt_);
}

void SignalBase::start_conversion(bool delayed_start)
{
	if (delayed_start) {
		delayed_conversion_starter_.start();
		return;
	}

	stop_conversion();

	if (converted_data_)
		converted_data_->clear();
	samples_cleared();

	conversion_interrupt_ = false;
	conversion_thread_ = std::thread(
		&SignalBase::conversion_thread_proc, this);
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
	if (converted_data_)
		converted_data_->clear();

	samples_cleared();
}

void SignalBase::on_samples_added(QObject* segment, uint64_t start_sample,
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

	data::Segment* s = qobject_cast<data::Segment*>(segment);
	samples_added(s->segment_id(), start_sample, end_sample);
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
