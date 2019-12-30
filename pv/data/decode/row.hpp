/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DATA_DECODE_ROW_HPP
#define PULSEVIEW_PV_DATA_DECODE_ROW_HPP

#include <vector>

#include <pv/data/decode/annotation.hpp>
#include <pv/data/decode/decoder.hpp>

struct srd_decoder;
struct srd_decoder_annotation_row;

namespace pv {
namespace data {
namespace decode {

struct AnnotationClass;
class Decoder;

class Row
{
public:
	Row();

	Row(uint32_t index, Decoder* decoder,
		const srd_decoder_annotation_row* srd_row = nullptr);

	const Decoder* decoder() const;
	const srd_decoder_annotation_row* get_srd_row() const;

	const QString title() const;
	const QString description() const;
	vector<AnnotationClass*> ann_classes() const;
	uint32_t index() const;

	bool visible() const;
	void set_visible(bool visible);

	bool has_hidden_classes() const;

	bool operator<(const Row& other) const;
	bool operator==(const Row& other) const;

private:
	uint32_t index_;
	Decoder* decoder_;
	const srd_decoder_annotation_row* srd_row_;
	bool visible_;
};

}  // namespace decode
}  // namespace data
}  // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODE_ROW_HPP
