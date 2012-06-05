/*
 * This file is part of the sigrok project.
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

#include "logicdatasnapshot.h"

#include <assert.h>

#include <QDebug>

LogicDataSnapshot::LogicDataSnapshot(
	const sr_datafeed_logic &logic) :
	DataSnapshot(logic.unitsize)
{
	append_payload(logic);
}

void LogicDataSnapshot::append_payload(
	const sr_datafeed_logic &logic)
{
	assert(_unit_size == logic.unitsize);

	qDebug() << "SR_DF_LOGIC (length =" << logic.length
		<< ", unitsize = " << logic.unitsize << ")";

	append_data(logic.data, logic.length);
}
