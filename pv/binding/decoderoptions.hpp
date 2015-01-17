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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_BINDING_DECODEROPTIONS_H
#define PULSEVIEW_PV_BINDING_DECODEROPTIONS_H

#include "binding.hpp"

#include <pv/prop/property.hpp>

struct srd_decoder_option;

namespace pv {

namespace data {
class DecoderStack;
namespace decode {
class Decoder;
}
}

namespace binding {

class DecoderOptions : public Binding
{
public:
	DecoderOptions(std::shared_ptr<pv::data::DecoderStack> decoder_stack,
		std::shared_ptr<pv::data::decode::Decoder> decoder);

private:
	static std::shared_ptr<prop::Property> bind_enum(const QString &name,
		const srd_decoder_option *option,
		prop::Property::Getter getter, prop::Property::Setter setter);

	Glib::VariantBase getter(const char *id);

	void setter(const char *id, Glib::VariantBase value);

private:
	std::shared_ptr<pv::data::DecoderStack> decoder_stack_;
	std::shared_ptr<pv::data::decode::Decoder> decoder_;
};

} // binding
} // pv

#endif // PULSEVIEW_PV_BINDING_DECODEROPTIONS_H
