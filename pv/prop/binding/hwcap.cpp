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

#include <boost/bind.hpp>

#include "hwcap.h"

#include <pv/prop/enum.h>

using namespace boost;
using namespace std;

namespace pv {
namespace prop {
namespace binding {

HwCap::HwCap(struct sr_dev_inst *sdi) :
	_sdi(sdi)
{
	const int *hwcaps;

	if ((sr_info_get(sdi->driver, SR_DI_HWCAPS, (const void **)&hwcaps,
			sdi) != SR_OK) || !hwcaps)
		/* Driver supports no device instance options. */
		return;

	for (int cap = 0; hwcaps[cap]; cap++) {
		const struct sr_hwcap_option *const hwo =
			sr_devopt_get(hwcaps[cap]);

		if (!hwo)
			continue;

		switch(hwo->hwcap)
		{
		case SR_HWCAP_PATTERN_MODE:
			bind_stropt(hwo, SR_DI_PATTERNS,
				SR_HWCAP_PATTERN_MODE);
			break;

		case SR_HWCAP_BUFFERSIZE:
			bind_buffer_size(hwo);
			break;

		case SR_HWCAP_TIMEBASE:
			bind_time_base(hwo);
			break;

		case SR_HWCAP_TRIGGER_SOURCE:
			bind_stropt(hwo, SR_DI_TRIGGER_SOURCES,
				SR_HWCAP_TRIGGER_SOURCE);
			break;

		case SR_HWCAP_FILTER:
			bind_stropt(hwo, SR_DI_FILTERS, SR_HWCAP_FILTER);
			break;

		case SR_HWCAP_VDIV:
			bind_vdiv(hwo);
			break;

		case SR_HWCAP_COUPLING:
			bind_stropt(hwo, SR_DI_COUPLING, SR_HWCAP_FILTER);
			break;
		}
	}
}

void HwCap::expose_enum(const struct sr_hwcap_option *hwo,
	const vector< pair<const void*, QString> > &values, int opt)
{
	_properties.push_back(shared_ptr<Property>(
		new Enum(QString(hwo->shortname), values,
			function<const void* ()>(),
			bind(sr_dev_config_set, _sdi, opt, _1))));
}

void HwCap::bind_stropt(const struct sr_hwcap_option *hwo, int id, int opt)
{
	const char **stropts;
	if (sr_info_get(_sdi->driver, id,
		(const void **)&stropts, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; stropts[i]; i++)
		values.push_back(make_pair(stropts[i], stropts[i]));

	expose_enum(hwo, values, opt);
}

void HwCap::bind_buffer_size(const struct sr_hwcap_option *hwo)
{
	const uint64_t *sizes;
	if (sr_info_get(_sdi->driver, SR_DI_BUFFERSIZES,
			(const void **)&sizes, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; sizes[i]; i++)
		values.push_back(make_pair(sizes + i,
			QString("%1").arg(sizes[i])));

	expose_enum(hwo, values, SR_HWCAP_BUFFERSIZE);
}

void HwCap::bind_time_base(const struct sr_hwcap_option *hwo)
{
	struct sr_rational *timebases;
	if (sr_info_get(_sdi->driver, SR_DI_TIMEBASES,
			(const void **)&timebases, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; timebases[i].p && timebases[i].q; i++)
		values.push_back(make_pair(timebases + i,
			QString(sr_period_string(
				timebases[i].p * timebases[i].q))));

	expose_enum(hwo, values, SR_HWCAP_TIMEBASE);
}

void HwCap::bind_vdiv(const struct sr_hwcap_option *hwo)
{
	struct sr_rational *vdivs;
	if (sr_info_get(_sdi->driver, SR_DI_VDIVS,
			(const void **)&vdivs, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; vdivs[i].p && vdivs[i].q; i++)
		values.push_back(make_pair(vdivs + i,
			QString(sr_voltage_string(vdivs + i))));

	expose_enum(hwo, values, SR_HWCAP_VDIV);
}

} // binding
} // prop
} // pv

