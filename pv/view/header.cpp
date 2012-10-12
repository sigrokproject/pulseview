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

#include "header.h"
#include "view.h"

#include "../../signal.h"
#include "../../sigsession.h"

#include <assert.h>

#include <boost/foreach.hpp>

#include <QPainter>
#include <QRect>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

Header::Header(View &parent) :
	QWidget(&parent),
	_view(parent)
{
}

void Header::paintEvent(QPaintEvent *event)
{
	const int w = width();
	const vector< shared_ptr<Signal> > &sigs =
		_view.session().get_signals();

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	int offset = -_view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect label_rect(0, offset, w, View::SignalHeight);
		s->paint_label(painter, label_rect);

		offset += View::SignalHeight;
	}

	painter.end();
}

} // namespace view
} // namespace pv
