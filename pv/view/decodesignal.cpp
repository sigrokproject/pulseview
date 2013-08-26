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

#include "decodesignal.h"

#include <pv/data/decoder.h>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

DecodeSignal::DecodeSignal(pv::SigSession &session,
	boost::shared_ptr<pv::data::Decoder> decoder) :
	Trace(session, QString(decoder->get_decoder()->name)),
	_decoder(decoder)
{
	_colour = Qt::red;
}

void DecodeSignal::init_context_bar_actions(QWidget *parent)
{
	(void)parent;
}

bool DecodeSignal::enabled() const
{
	return true;
}

void DecodeSignal::paint(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

const list<QAction*> DecodeSignal::get_context_bar_actions()
{
	list<QAction*> actions;
	return actions;
}

} // namespace view
} // namespace pv
