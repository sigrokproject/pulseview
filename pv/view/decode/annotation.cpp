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

extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include "annotation.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {
namespace decode {

Annotation::Annotation(const srd_proto_data *const pdata) :
	_start_sample(pdata->start_sample),
	_end_sample(pdata->end_sample)
{
	const char *const *annotations = (char**)pdata->data;
	while(*annotations) {
		_annotations.push_back(QString(*annotations));
		annotations++;
	}
}

} // namespace decode
} // namespace view
} // namespace pv
