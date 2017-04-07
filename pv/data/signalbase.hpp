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

#ifndef PULSEVIEW_PV_DATA_SIGNALBASE_HPP
#define PULSEVIEW_PV_DATA_SIGNALBASE_HPP

#include <thread>

#include <QColor>
#include <QObject>
#include <QSettings>
#include <QString>

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::shared_ptr;

namespace sigrok {
class Channel;
}

namespace pv {
namespace data {

class Analog;
class DecoderStack;
class Logic;
class SignalData;

class SignalBase : public QObject
{
	Q_OBJECT

public:
	enum ChannelType {
		AnalogChannel = 1,
		LogicChannel,
		DecodeChannel,
		A2LChannel,  // Analog converted to logic, joint representation
		MathChannel
	};

	enum ConversionType {
		NoConversion = 0,
		A2LConversionByTreshold = 1,
		A2LConversionBySchmittTrigger = 2
	};

private:
	static const int ColourBGAlpha;

public:
	SignalBase(shared_ptr<sigrok::Channel> channel, ChannelType channel_type);
	virtual ~SignalBase();

public:
	/**
	 * Returns the underlying SR channel.
	 */
	shared_ptr<sigrok::Channel> channel() const;

	/**
	 * Returns enabled status of this channel.
	 */
	bool enabled() const;

	/**
	 * Sets the enabled status of this channel.
	 * @param value Boolean value to set.
	 */
	void set_enabled(bool value);

	/**
	 * Gets the type of this channel.
	 */
	ChannelType type() const;

	/**
	 * Gets the index number of this channel.
	 */
	unsigned int index() const;

	/**
	 * Gets the name of this signal.
	 */
	QString name() const;

	/**
	 * Gets the internal name of this signal, i.e. how the device calls it.
	 */
	QString internal_name() const;

	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	/**
	 * Get the colour of the signal.
	 */
	QColor colour() const;

	/**
	 * Set the colour of the signal.
	 */
	void set_colour(QColor colour);

	/**
	 * Get the background colour of the signal.
	 */
	QColor bgcolour() const;

	/**
	 * Sets the internal data object.
	 */
	void set_data(shared_ptr<pv::data::SignalData> data);

	/**
	 * Get the internal data as analog data object in case of analog type.
	 */
	shared_ptr<pv::data::Analog> analog_data() const;

	/**
	 * Get the internal data as logic data object in case of logic type.
	 */
	shared_ptr<pv::data::Logic> logic_data() const;

	/**
	 * Changes the kind of conversion performed on this channel.
	 */
	void set_conversion_type(ConversionType t);

#ifdef ENABLE_DECODE
	virtual bool is_decode_signal() const;

	virtual shared_ptr<pv::data::DecoderStack> decoder_stack() const;
#endif

	void save_settings(QSettings &settings) const;

	void restore_settings(QSettings &settings);

private:
	uint8_t convert_a2l_threshold(float threshold, float value);
	uint8_t convert_a2l_schmitt_trigger(float lo_thr, float hi_thr,
		float value, uint8_t &state);

	void conversion_thread_proc(QObject* segment, uint64_t start_sample,
		uint64_t end_sample);

Q_SIGNALS:
	void enabled_changed(const bool &value);

	void name_changed(const QString &name);

	void colour_changed(const QColor &colour);

	void conversion_type_changed(const ConversionType t);

	void samples_cleared();

	void samples_added(QObject* segment, uint64_t start_sample,
		uint64_t end_sample);

private Q_SLOTS:
	void on_samples_cleared();

	void on_samples_added(QObject* segment, uint64_t start_sample,
		uint64_t end_sample);

	void on_capture_state_changed(int state);

protected:
	shared_ptr<sigrok::Channel> channel_;
	ChannelType channel_type_;
	shared_ptr<pv::data::SignalData> data_;
	shared_ptr<pv::data::SignalData> converted_data_;
	int conversion_type_;

	std::thread conversion_thread_;

	QString internal_name_, name_;
	QColor colour_, bgcolour_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_SIGNALBASE_HPP
