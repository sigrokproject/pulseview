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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "analog.hpp"
#include "logic.hpp"
#include "signalbase.hpp"
#include "signaldata.hpp"
#include "decode/row.hpp"

#include <pv/binding/decoder.hpp>

using std::dynamic_pointer_cast;
using std::shared_ptr;

using sigrok::Channel;
using sigrok::ChannelType;

namespace pv {
namespace data {

const int SignalBase::ColourBGAlpha = 8*256/100;

SignalBase::SignalBase(shared_ptr<sigrok::Channel> channel) :
	channel_(channel)
{
	if (channel_)
		internal_name_ = QString::fromStdString(channel_->name());
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

const ChannelType *SignalBase::type() const
{
	return (channel_) ? channel_->type() : nullptr;
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
	data_ = data;
}

shared_ptr<data::Analog> SignalBase::analog_data() const
{
	if (type() == ChannelType::ANALOG)
		return dynamic_pointer_cast<data::Analog>(data_);
	else
		return shared_ptr<data::Analog>();
}

shared_ptr<data::Logic> SignalBase::logic_data() const
{
	if (type() == ChannelType::LOGIC)
		return dynamic_pointer_cast<data::Logic>(data_);
	else
		return shared_ptr<data::Logic>();
}

#ifdef ENABLE_DECODE
bool SignalBase::is_decode_signal() const
{
	return (decoder_stack_ != nullptr);
}

std::shared_ptr<pv::data::DecoderStack> SignalBase::decoder_stack() const
{
	return decoder_stack_;
}

void SignalBase::set_decoder_stack(std::shared_ptr<pv::data::DecoderStack>
	decoder_stack)
{
	decoder_stack_ = decoder_stack;
}
#endif

} // namespace data
} // namespace pv
