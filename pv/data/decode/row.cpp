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

#include <cassert>

#include "decoder.hpp"
#include "row.hpp"

#include <libsigrokdecode/libsigrokdecode.h>

namespace pv {
namespace data {
namespace decode {

Row::Row() :
	index_(0),
	decoder_(nullptr),
	srd_row_(nullptr),
	visible_(true)
{
}

Row::Row(uint32_t index, Decoder* decoder, const srd_decoder_annotation_row* srd_row) :
	index_(index),
	decoder_(decoder),
	srd_row_(srd_row),
	visible_(true)
{
}

const Decoder* Row::decoder() const
{
	return decoder_;
}

const srd_decoder_annotation_row* Row::get_srd_row() const
{
	return srd_row_;
}

const QString Row::title() const
{
	if (decoder_ && decoder_->name() && srd_row_ && srd_row_->desc)
		return QString("%1: %2")
			.arg(QString::fromUtf8(decoder_->name()),
			     QString::fromUtf8(srd_row_->desc));
	if (decoder_ && decoder_->name())
		return QString::fromUtf8(decoder_->name());
	if (srd_row_ && srd_row_->desc)
		return QString::fromUtf8(srd_row_->desc);

	return QString();
}

const QString Row::description() const
{
	if (srd_row_ && srd_row_->desc)
		return QString::fromUtf8(srd_row_->desc);
	return QString();
}

vector<AnnotationClass*> Row::ann_classes() const
{
	assert(decoder_);

	vector<AnnotationClass*> result;

	if (!srd_row_) {
		if (index_ == 0) {
			// When operating as the fallback row, all annotation classes belong to it
			return decoder_->ann_classes();
		}
		return result;
	}

	for (GSList *l = srd_row_->ann_classes; l; l = l->next) {
		size_t class_id = (size_t)l->data;
		result.push_back(decoder_->get_ann_class_by_id(class_id));
	}

	return result;
}

uint32_t Row::index() const
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

bool Row::has_hidden_classes() const
{
	for (const AnnotationClass* c : ann_classes())
		if (!c->visible)
			return true;

	return false;
}

bool Row::operator<(const Row& other) const
{
	return (decoder_ < other.decoder_) ||
		(decoder_ == other.decoder_ && srd_row_ < other.srd_row_);
}

bool Row::operator==(const Row& other) const
{
	return ((decoder_ == other.decoder()) && (srd_row_ == other.srd_row_));
}

}  // namespace decode
}  // namespace data
}  // namespace pv
