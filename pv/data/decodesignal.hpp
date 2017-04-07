/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
#define PULSEVIEW_PV_DATA_DECODESIGNAL_HPP

#include <libsigrokdecode/libsigrokdecode.h>

#include <pv/data/signalbase.hpp>

using std::shared_ptr;

namespace pv {
namespace data {

class DecoderStack;
class Logic;
class SignalData;

class DecodeSignal : public SignalBase
{
	Q_OBJECT

public:
	DecodeSignal(shared_ptr<pv::data::DecoderStack> decoder_stack);
	virtual ~DecodeSignal();

	bool is_decode_signal() const;
	shared_ptr<pv::data::DecoderStack> decoder_stack() const;

	void stack_decoder(srd_decoder *decoder);
	void remove_decoder(int index);
	bool toggle_decoder_visibility(int index);

Q_SIGNALS:
	void new_annotations();

private Q_SLOTS:
	void on_new_annotations();

private:
	shared_ptr<pv::data::DecoderStack> decoder_stack_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
