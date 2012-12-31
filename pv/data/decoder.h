/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DATA_DECODER_H
#define PULSEVIEW_PV_DATA_DECODER_H

#include "signaldata.h"

struct srd_decoder;

namespace pv {
namespace data {

class Decoder : public SignalData
{
public:
	Decoder(const srd_decoder *const dec);

	const srd_decoder* get_decoder() const;

	void clear_snapshots();

private:
	const srd_decoder *const _decoder;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODER_H
