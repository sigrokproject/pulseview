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

#include <libsigrokdecode/libsigrokdecode.h>

#include "decoderoptions.h"

#include <boost/none_t.hpp>

#include <pv/data/decoderstack.h>
#include <pv/data/decode/decoder.h>
#include <pv/prop/double.h>
#include <pv/prop/enum.h>
#include <pv/prop/int.h>
#include <pv/prop/string.h>

using boost::none;
using std::make_pair;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace pv {
namespace prop {
namespace binding {

DecoderOptions::DecoderOptions(
	shared_ptr<pv::data::DecoderStack> decoder_stack,
	shared_ptr<data::decode::Decoder> decoder) :
	_decoder_stack(decoder_stack),
	_decoder(decoder)
{
	assert(_decoder);

	const srd_decoder *const dec = _decoder->decoder();
	assert(dec);

	for (GSList *l = dec->options; l; l = l->next)
	{
		const srd_decoder_option *const opt =
			(srd_decoder_option*)l->data;

		const QString name = QString::fromUtf8(opt->desc);

		const Property::Getter get = [&, opt]() {
			return getter(opt->id); };
		const Property::Setter set = [&, opt](Glib::VariantBase value) {
			setter(opt->id, value); };

		shared_ptr<Property> prop;

		if (opt->values)
			prop = bind_enum(name, opt, get, set);
		else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d")))
			prop = shared_ptr<Property>(new Double(name, 2, "",
				none, none, get, set));
		else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x")))
			prop = shared_ptr<Property>(
				new Int(name, "", none, get, set));
		else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s")))
			prop = shared_ptr<Property>(
				new String(name, get, set));
		else
			continue;

		_properties.push_back(prop);
	}
}

shared_ptr<Property> DecoderOptions::bind_enum(
	const QString &name, const srd_decoder_option *option,
	Property::Getter getter, Property::Setter setter)
{
	vector< pair<Glib::VariantBase, QString> > values;
	for (GSList *l = option->values; l; l = l->next) {
		Glib::VariantBase var = Glib::VariantBase((GVariant*)l->data, true);
		values.push_back(make_pair(var, print_gvariant(var)));
	}

	return shared_ptr<Property>(new Enum(name, values, getter, setter));
}

Glib::VariantBase DecoderOptions::getter(const char *id)
{
	GVariant *val = NULL;

	assert(_decoder);

	// Get the value from the hash table if it is already present
	const map<string, GVariant*>& options = _decoder->options();
	const auto iter = options.find(id);

	if (iter != options.end())
		val = (*iter).second;
	else
	{
		assert(_decoder->decoder());

		// Get the default value if not
		for (GSList *l = _decoder->decoder()->options; l; l = l->next)
		{
			const srd_decoder_option *const opt =
				(srd_decoder_option*)l->data;
			if (strcmp(opt->id, id) == 0) {
				val = opt->def;
				break;
			}
		}
	}

	if (val)
		return Glib::VariantBase(val, true);
	else
		return Glib::VariantBase();
}

void DecoderOptions::setter(const char *id, Glib::VariantBase value)
{
	assert(_decoder);
	_decoder->set_option(id, value.gobj());

	assert(_decoder_stack);
	_decoder_stack->begin_decode();
}

} // binding
} // prop
} // pv
