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

#include "logic.hpp"
#include "logicsegment.hpp"
#include "decodesignal.hpp"
#include "signaldata.hpp"

#include <pv/binding/decoder.hpp>
#include <pv/data/decode/decoder.hpp>
#include <pv/data/decoderstack.hpp>
#include <pv/session.hpp>

using std::make_shared;
using std::shared_ptr;
using pv::data::decode::Decoder;

namespace pv {
namespace data {

DecodeSignal::DecodeSignal(shared_ptr<pv::data::DecoderStack> decoder_stack) :
	SignalBase(nullptr, SignalBase::DecodeChannel),
	decoder_stack_(decoder_stack)
{
	set_name(QString::fromUtf8(decoder_stack_->stack().front()->decoder()->name));

	connect(decoder_stack_.get(), SIGNAL(new_annotations()),
		this, SLOT(on_new_annotations()));
}

DecodeSignal::~DecodeSignal()
{
}

bool DecodeSignal::is_decode_signal() const
{
	return true;
}

shared_ptr<pv::data::DecoderStack> DecodeSignal::decoder_stack() const
{
	return decoder_stack_;
}

void DecodeSignal::stack_decoder(srd_decoder *decoder)
{
	assert(decoder);
	assert(decoder_stack);
	decoder_stack_->push(make_shared<data::decode::Decoder>(decoder));
	decoder_stack_->begin_decode();
}

void DecodeSignal::remove_decoder(int index)
{
	decoder_stack_->remove(index);
	decoder_stack_->begin_decode();
}

bool DecodeSignal::toggle_decoder_visibility(int index)
{
	const list< shared_ptr<Decoder> > stack(decoder_stack_->stack());

	auto iter = stack.cbegin();
	for (int i = 0; i < index; i++, iter++)
		assert(iter != stack.end());

	shared_ptr<Decoder> dec = *iter;

	// Toggle decoder visibility
	bool state = false;
	if (dec) {
		state = !dec->shown();
		dec->show(state);
	}

	return state;
}

void DecodeSignal::on_new_annotations()
{
	// Forward the signal to the frontend
	new_annotations();
}

} // namespace data
} // namespace pv
