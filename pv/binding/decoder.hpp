/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_BINDING_DECODER_HPP
#define PULSEVIEW_PV_BINDING_DECODER_HPP

#include "binding.hpp"

#include <pv/prop/property.hpp>

using std::shared_ptr;

struct srd_decoder_option;

namespace pv {

namespace data {
class DecoderStack;
namespace decode {
class Decoder;
}
}

namespace binding {

class Decoder : public Binding
{
public:
	Decoder(shared_ptr<pv::data::DecoderStack> decoder_stack,
		shared_ptr<pv::data::decode::Decoder> decoder);

private:
	static shared_ptr<prop::Property> bind_enum(const QString &name,
		const srd_decoder_option *option,
		prop::Property::Getter getter, prop::Property::Setter setter);

	Glib::VariantBase getter(const char *id);

	void setter(const char *id, Glib::VariantBase value);

private:
	shared_ptr<pv::data::DecoderStack> decoder_stack_;
	shared_ptr<pv::data::decode::Decoder> decoder_;
};

} // binding
} // pv

#endif // PULSEVIEW_PV_BINDING_DECODER_HPP
