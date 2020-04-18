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

#ifndef PULSEVIEW_PV_VIEW_DECODE_ANNOTATION_HPP
#define PULSEVIEW_PV_VIEW_DECODE_ANNOTATION_HPP

#include <cstdint>
#include <vector>

#include <QString>

using std::vector;

struct srd_proto_data;

namespace pv {
namespace data {
namespace decode {

class Row;
class RowData;

class Annotation
{
public:
	typedef uint32_t Class;

public:
	Annotation(uint64_t start_sample, uint64_t end_sample,
		const vector<QString>* texts, Class ann_class_id, const RowData *data);
	Annotation(Annotation&& a);
	Annotation& operator=(Annotation&& a);
	~Annotation();

	uint64_t start_sample() const;
	uint64_t end_sample() const;

	Class ann_class_id() const;
	const QString ann_class_name() const;

	const vector<QString>* annotations() const;
	const QString longest_annotation() const;
	const Row* row() const;

	bool operator<(const Annotation &other) const;

private:
	uint64_t start_sample_;
	uint64_t end_sample_;
	const vector<QString>* texts_;
	Class ann_class_id_;
	const RowData* data_;
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_DECODE_ANNOTATION_HPP
