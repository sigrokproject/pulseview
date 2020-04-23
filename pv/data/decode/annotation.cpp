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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include <cassert>
#include <vector>

#include <pv/data/decode/annotation.hpp>
#include <pv/data/decode/decoder.hpp>
#include "pv/data/decode/rowdata.hpp"

using std::vector;

namespace pv {
namespace data {
namespace decode {

Annotation::Annotation(uint64_t start_sample, uint64_t end_sample,
	const vector<QString>* texts, Class ann_class_id, const RowData *data) :
	start_sample_(start_sample),
	end_sample_(end_sample),
	texts_(texts),
	ann_class_id_(ann_class_id),
	data_(data)
{
}

Annotation::Annotation(Annotation&& a) :
	start_sample_(a.start_sample_),
	end_sample_(a.end_sample_),
	texts_(a.texts_),
	ann_class_id_(a.ann_class_id_),
	data_(a.data_)
{
}

Annotation& Annotation::operator=(Annotation&& a)
{
	if (&a != this) {
		start_sample_ = a.start_sample_;
		end_sample_ = a.end_sample_;
		texts_ = a.texts_;
		ann_class_id_ = a.ann_class_id_;
		data_ = a.data_;
	}

	return *this;
}

const RowData* Annotation::row_data() const
{
	return data_;
}

const Row* Annotation::row() const
{
	return data_->row();
}

uint64_t Annotation::start_sample() const
{
	return start_sample_;
}

uint64_t Annotation::end_sample() const
{
	return end_sample_;
}

uint64_t Annotation::length() const
{
	return end_sample_ - start_sample_;
}

Annotation::Class Annotation::ann_class_id() const
{
	return ann_class_id_;
}

const QString Annotation::ann_class_name() const
{
	const AnnotationClass* ann_class =
		data_->row()->decoder()->get_ann_class_by_id(ann_class_id_);

	return QString(ann_class->name);
}

const QString Annotation::ann_class_description() const
{
	const AnnotationClass* ann_class =
		data_->row()->decoder()->get_ann_class_by_id(ann_class_id_);

	return QString(ann_class->description);
}

const vector<QString>* Annotation::annotations() const
{
	return texts_;
}

const QString Annotation::longest_annotation() const
{
	return texts_->front();
}

const QColor Annotation::color() const
{
	return data_->row()->get_class_color(ann_class_id_);
}

const QColor Annotation::bright_color() const
{
	return data_->row()->get_bright_class_color(ann_class_id_);
}

const QColor Annotation::dark_color() const
{
	return data_->row()->get_dark_class_color(ann_class_id_);
}

bool Annotation::operator<(const Annotation &other) const
{
	return (start_sample_ < other.start_sample_);
}

} // namespace decode
} // namespace data
} // namespace pv
