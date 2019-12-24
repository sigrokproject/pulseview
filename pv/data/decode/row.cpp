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

#include "decoder.hpp"
#include "row.hpp"

#include <libsigrokdecode/libsigrokdecode.h>

namespace pv {
namespace data {
namespace decode {

Row::Row() :
	decoder_(nullptr),
	row_(nullptr)
{
}

Row::Row(int index, const Decoder* decoder, const srd_decoder_annotation_row* row) :
	index_(index),
	decoder_(decoder),
	row_(row),
	visible_(true)
{
}

const Decoder* Row::decoder() const
{
	return decoder_;
}

const srd_decoder_annotation_row* Row::srd_row() const
{
	return row_;
}

const QString Row::title() const
{
	if (decoder_ && decoder_->name() && row_ && row_->desc)
		return QString("%1: %2")
			.arg(QString::fromUtf8(decoder_->name()),
			     QString::fromUtf8(row_->desc));
	if (decoder_ && decoder_->name())
		return QString::fromUtf8(decoder_->name());
	if (row_ && row_->desc)
		return QString::fromUtf8(row_->desc);
	return QString();
}

const QString Row::class_name() const
{
	if (row_ && row_->desc)
		return QString::fromUtf8(row_->desc);
	return QString();
}

int Row::index() const
{
	return index_;
}

bool Row::visible() const
{
	return visible_;
}

void Row::set_visible(bool visible)
{
	visible_ = visible;
}

bool Row::operator<(const Row& other) const
{
	return (decoder_ < other.decoder_) ||
		(decoder_ == other.decoder_ && row_ < other.row_);
}

bool Row::operator==(const Row& other) const
{
	return ((decoder_ == other.decoder()) && (row_ == other.srd_row()));
}

}  // namespace decode
}  // namespace data
}  // namespace pv
