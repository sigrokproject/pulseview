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

#ifndef PULSEVIEW_PV_DATA_DECODE_DECODER_HPP
#define PULSEVIEW_PV_DATA_DECODE_DECODER_HPP

#include <map>
#include <memory>
#include <set>

#include <glib.h>

struct srd_decoder;
struct srd_decoder_inst;
struct srd_channel;
struct srd_session;

namespace pv {

namespace view {
class LogicSignal;
}

namespace data {

class Logic;

namespace decode {

class Decoder
{
public:
	Decoder(const srd_decoder *const dec);

	virtual ~Decoder();

	const srd_decoder* decoder() const;

	bool shown() const;
	void show(bool show = true);

	const std::map<const srd_channel*,
		std::shared_ptr<view::LogicSignal> >& channels() const;
	void set_channels(std::map<const srd_channel*,
		std::shared_ptr<view::LogicSignal> > channels);

	const std::map<std::string, GVariant*>& options() const;

	void set_option(const char *id, GVariant *value);

	bool have_required_channels() const;

	srd_decoder_inst* create_decoder_inst(
		srd_session *session) const;

	std::set< std::shared_ptr<pv::data::Logic> > get_data();

private:
	const srd_decoder *const decoder_;

	bool shown_;

	std::map<const srd_channel*, std::shared_ptr<pv::view::LogicSignal> >
		channels_;
	std::map<std::string, GVariant*> options_;
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODE_DECODER_HPP
