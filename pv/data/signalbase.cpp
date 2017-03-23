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
#include "logic.hpp"
#include "logicsegment.hpp"
#include "signalbase.hpp"
#include "signaldata.hpp"
#include "decode/row.hpp"

#include <pv/session.hpp>
#include <pv/binding/decoder.hpp>

using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::tie;

namespace pv {
namespace data {

const int SignalBase::ColourBGAlpha = 8 * 256 / 100;

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
	// Wait for the currently ongoing conversion to finish
	if (conversion_thread_.joinable())
		conversion_thread_.join();
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
	return (channel_) ? channel_->index() : (unsigned int)-1;
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
	if (data_ && channel_type_ == AnalogChannel) {
		shared_ptr<Analog> analog_data = dynamic_pointer_cast<Analog>(data_);

		disconnect(analog_data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		disconnect(analog_data.get(), SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(QObject*, uint64_t, uint64_t)));
	}

	data_ = data;

	if (data_ && channel_type_ == AnalogChannel) {
		shared_ptr<Analog> analog_data = dynamic_pointer_cast<Analog>(data_);

		connect(analog_data.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_samples_cleared()));
		connect(analog_data.get(), SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
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
		// Wait for the currently ongoing conversion to finish
		if (conversion_thread_.joinable())
			conversion_thread_.join();

		// Discard converted data
		converted_data_.reset();
	}

	conversion_type_ = t;

	if ((channel_type_ == AnalogChannel) &&
		((conversion_type_ == A2LConversionByTreshold) ||
		(conversion_type_ == A2LConversionBySchmittTrigger))) {

		shared_ptr<Analog> analog_data = dynamic_pointer_cast<Analog>(data_);

		if (analog_data->analog_segments().size() > 0) {
			AnalogSegment *asegment = analog_data->analog_segments().front().get();

			// Begin conversion of existing sample data
			// TODO Support for multiple segments is missing
			on_samples_added(asegment, 0, 0);
		}
	}

	conversion_type_changed(t);
}

#ifdef ENABLE_DECODE
bool SignalBase::is_decode_signal() const
{
	return (decoder_stack_ != nullptr);
}

shared_ptr<pv::data::DecoderStack> SignalBase::decoder_stack() const
{
	return decoder_stack_;
}

void SignalBase::set_decoder_stack(shared_ptr<pv::data::DecoderStack>
	decoder_stack)
{
	decoder_stack_ = decoder_stack;
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

void SignalBase::conversion_thread_proc(QObject* segment, uint64_t start_sample,
	uint64_t end_sample)
{
	const uint64_t block_size = 4096;

	// TODO Support for multiple segments is missing

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

		// start_sample=end_sample=0 means we need to figure out the unprocessed range
		if ((start_sample == 0) && (end_sample == 0)) {
			start_sample = lsegment->get_sample_count();
			end_sample = asegment->get_sample_count();
		}

		if (start_sample == end_sample)
			return;  // Nothing to do

		float min_v, max_v;
		tie(min_v, max_v) = asegment->get_min_max();

		vector<uint8_t> lsamples;
		lsamples.reserve(block_size);

		uint64_t i = start_sample;

		if (conversion_type_ == A2LConversionByTreshold) {
			const float threshold = (min_v + max_v) * 0.5;  // middle between min and max

			// Convert as many sample blocks as we can
			while ((end_sample - i) > block_size) {
				const float* asamples = asegment->get_samples(i, i + block_size);
				for (uint32_t j = 0; j < block_size; j++)
					lsamples.push_back(convert_a2l_threshold(threshold, asamples[j]));
				lsegment->append_payload(lsamples.data(), lsamples.size());
				i += block_size;
				lsamples.clear();
				delete[] asamples;
			}

			// Convert remaining samples
			const float* asamples = asegment->get_samples(i, end_sample);
			for (uint32_t j = 0; j < (end_sample - i); j++)
				lsamples.push_back(convert_a2l_threshold(threshold, asamples[j]));
			lsegment->append_payload(lsamples.data(), lsamples.size());
			delete[] asamples;
		}

		if (conversion_type_ == A2LConversionBySchmittTrigger) {
			const float amplitude = max_v - min_v;
			const float lo_thr = min_v + (amplitude * 0.1);  // 10% above min
			const float hi_thr = max_v - (amplitude * 0.1);  // 10% below max
			uint8_t state = 0;  // TODO Use value of logic sample n-1 instead of 0

			// Convert as many sample blocks as we can
			while ((end_sample - i) > block_size) {
				const float* asamples = asegment->get_samples(i, i + block_size);
				for (uint32_t j = 0; j < block_size; j++)
					lsamples.push_back(convert_a2l_schmitt_trigger(lo_thr, hi_thr, asamples[j], state));
				lsegment->append_payload(lsamples.data(), lsamples.size());
				i += block_size;
				lsamples.clear();
				delete[] asamples;
			}

			// Convert remaining samples
			const float* asamples = asegment->get_samples(i, end_sample);
			for (uint32_t j = 0; j < (end_sample - i); j++)
				lsamples.push_back(convert_a2l_schmitt_trigger(lo_thr, hi_thr, asamples[j], state));
			lsegment->append_payload(lsamples.data(), lsamples.size());
			delete[] asamples;
		}
	}
}

void SignalBase::on_samples_cleared()
{
	if (converted_data_)
		converted_data_->clear();
}

void SignalBase::on_samples_added(QObject* segment, uint64_t start_sample,
	uint64_t end_sample)
{
	(void)segment;
	(void)start_sample;
	(void)end_sample;

	if (conversion_type_ != NoConversion) {

		// Wait for the currently ongoing conversion to finish
		if (conversion_thread_.joinable())
			conversion_thread_.join();

		conversion_thread_ = std::thread(
			&SignalBase::conversion_thread_proc, this,
			segment, start_sample, end_sample);
	}
}

void SignalBase::on_capture_state_changed(int state)
{
	return;
	if (state == Session::Stopped) {
		// Make sure that all data is converted

		if ((channel_type_ == AnalogChannel) &&
			((conversion_type_ == A2LConversionByTreshold) ||
			(conversion_type_ == A2LConversionBySchmittTrigger))) {

			shared_ptr<Analog> analog_data = dynamic_pointer_cast<Analog>(data_);

			if (analog_data->analog_segments().size() > 0) {
				// TODO Support for multiple segments is missing
				AnalogSegment *asegment = analog_data->analog_segments().front().get();

				if (conversion_thread_.joinable())
					conversion_thread_.join();

				conversion_thread_ = std::thread(
					&SignalBase::conversion_thread_proc, this, asegment, 0, 0);
			}
		}
	}
}

} // namespace data
} // namespace pv
