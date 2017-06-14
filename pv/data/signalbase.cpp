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

#include <pv/binding/decoder.hpp>
#include <pv/session.hpp>

using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::tie;
using std::unique_lock;

namespace pv {
namespace data {

const int SignalBase::ColourBGAlpha = 8 * 256 / 100;
const uint64_t SignalBase::ConversionBlockSize = 4096;

SignalBase::SignalBase(shared_ptr<sigrok::Channel> channel, ChannelType channel_type) :
	channel_(channel),
	channel_type_(channel_type),
	conversion_type_(NoConversion)
{
	if (channel_)
		internal_name_ = QString::fromStdString(channel_->name());
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
	}

	data_ = data;

	if (data_) {
		connect(data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		connect(data.get(), SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(QObject*, uint64_t, uint64_t)));
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

	if (((conversion_type_ == A2LConversionByTreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger)))
		result = dynamic_pointer_cast<Logic>(converted_data_);

	return result;
}

void SignalBase::set_conversion_type(ConversionType t)
{
	if (conversion_type_ != NoConversion) {
		stop_conversion();

		// Discard converted data
		converted_data_.reset();
	}

	conversion_type_ = t;

	start_conversion();

	conversion_type_changed(t);
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
}

void SignalBase::restore_settings(QSettings &settings)
{
	set_name(settings.value("name").toString());
	set_enabled(settings.value("enabled").toBool());
	set_colour(settings.value("colour").value<QColor>());
	set_conversion_type((ConversionType)settings.value("conversion_type").toInt());
}

uint8_t SignalBase::convert_a2l_threshold(float threshold, float value)
{
	return (value >= threshold) ? 1 : 0;
}

uint8_t SignalBase::convert_a2l_schmitt_trigger(float lo_thr, float hi_thr,
	float value, uint8_t &state)
{
	if (value < lo_thr)
		state = 0;
	else if (value > hi_thr)
		state = 1;

	return state;
}

void SignalBase::conversion_thread_proc(QObject* segment)
{
	// TODO Support for multiple segments is missing

	uint64_t start_sample, end_sample;
	start_sample = end_sample = 0;

	do {
		if ((channel_type_ == AnalogChannel) &&
			((conversion_type_ == A2LConversionByTreshold) ||
			(conversion_type_ == A2LConversionBySchmittTrigger))) {

			AnalogSegment *asegment = qobject_cast<AnalogSegment*>(segment);

			// Create the logic data container if needed
			shared_ptr<Logic> logic_data;
			if (!converted_data_) {
				logic_data = make_shared<Logic>(1);  // Contains only one channel
				converted_data_ = logic_data;
			} else
				 logic_data = dynamic_pointer_cast<Logic>(converted_data_);

			// Create the initial logic data segment if needed
			if (logic_data->segments().size() == 0) {
				shared_ptr<LogicSegment> lsegment =
					make_shared<LogicSegment>(*logic_data.get(), 1, asegment->samplerate());
				logic_data->push_segment(lsegment);
			}

			LogicSegment *lsegment = dynamic_cast<LogicSegment*>(logic_data->segments().front().get());

			start_sample = lsegment->get_sample_count();
			end_sample = asegment->get_sample_count();

			if (end_sample > start_sample) {
				float min_v, max_v;
				tie(min_v, max_v) = asegment->get_min_max();

				vector<uint8_t> lsamples;
				lsamples.reserve(ConversionBlockSize);

				uint64_t i = start_sample;

				if (conversion_type_ == A2LConversionByTreshold) {
					const float threshold = (min_v + max_v) * 0.5;  // middle between min and max

					// Convert as many sample blocks as we can
					while ((end_sample - i) > ConversionBlockSize) {
						const float* asamples = asegment->get_samples(i, i + ConversionBlockSize);
						for (uint32_t j = 0; j < ConversionBlockSize; j++)
							lsamples.push_back(convert_a2l_threshold(threshold, asamples[j]));
						lsegment->append_payload(lsamples.data(), lsamples.size());
						samples_added(lsegment, i, i + ConversionBlockSize);
						i += ConversionBlockSize;
						lsamples.clear();
						delete[] asamples;
					}

					// Convert remaining samples
					const float* asamples = asegment->get_samples(i, end_sample);
					for (uint32_t j = 0; j < (end_sample - i); j++)
						lsamples.push_back(convert_a2l_threshold(threshold, asamples[j]));
					lsegment->append_payload(lsamples.data(), lsamples.size());
					samples_added(lsegment, i, end_sample);
					delete[] asamples;
				}

				if (conversion_type_ == A2LConversionBySchmittTrigger) {
					const float amplitude = max_v - min_v;
					const float lo_thr = min_v + (amplitude * 0.1);  // 10% above min
					const float hi_thr = max_v - (amplitude * 0.1);  // 10% below max
					uint8_t state = 0;  // TODO Use value of logic sample n-1 instead of 0

					// Convert as many sample blocks as we can
					while ((end_sample - i) > ConversionBlockSize) {
						const float* asamples = asegment->get_samples(i, i + ConversionBlockSize);
						for (uint32_t j = 0; j < ConversionBlockSize; j++)
							lsamples.push_back(convert_a2l_schmitt_trigger(lo_thr, hi_thr, asamples[j], state));
						lsegment->append_payload(lsamples.data(), lsamples.size());
						samples_added(lsegment, i, i + ConversionBlockSize);
						i += ConversionBlockSize;
						lsamples.clear();
						delete[] asamples;
					}

					// Convert remaining samples
					const float* asamples = asegment->get_samples(i, end_sample);
					for (uint32_t j = 0; j < (end_sample - i); j++)
						lsamples.push_back(convert_a2l_schmitt_trigger(lo_thr, hi_thr, asamples[j], state));
					lsegment->append_payload(lsamples.data(), lsamples.size());
					samples_added(lsegment, i, end_sample);
					delete[] asamples;
				}

				// If acquisition is ongoing, start-/endsample may have changed
				end_sample = asegment->get_sample_count();
			}
		}

		if (!conversion_interrupt_ && (start_sample == end_sample)) {
			unique_lock<mutex> input_lock(conversion_input_mutex_);
			conversion_input_cond_.wait(input_lock);
		}
	} while (!conversion_interrupt_);
}

void SignalBase::start_conversion()
{
	stop_conversion();

	if ((channel_type_ == AnalogChannel) &&
		((conversion_type_ == A2LConversionByTreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger))) {

		shared_ptr<Analog> analog_data = dynamic_pointer_cast<Analog>(data_);

		if (analog_data->analog_segments().size() > 0) {
			// TODO Support for multiple segments is missing
			AnalogSegment *asegment = analog_data->analog_segments().front().get();

			conversion_interrupt_ = false;
			conversion_thread_ = std::thread(
				&SignalBase::conversion_thread_proc, this, asegment);
		}
	}
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
			// Start the conversion thread
			start_conversion();
		}
	}

	samples_added(segment, start_sample, end_sample);
}

void SignalBase::on_capture_state_changed(int state)
{
	if (state == Session::Running) {
		if (conversion_type_ != NoConversion) {
			// Restart conversion
			stop_conversion();
			start_conversion();
		}
	}
}

} // namespace data
} // namespace pv
