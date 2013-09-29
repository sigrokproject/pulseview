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

#include <extdef.h>

#include <QAction>

#include "decodesignal.h"

#include <pv/sigsession.h>
#include <pv/data/decoder.h>
#include <pv/view/view.h>
#include <pv/view/decode/annotation.h>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const QColor DecodeSignal::DecodeColours[4] = {
	QColor(0xEF, 0x29, 0x29),	// Red
	QColor(0xFC, 0xE9, 0x4F),	// Yellow
	QColor(0x8A, 0xE2, 0x34),	// Green
	QColor(0x72, 0x9F, 0xCF)	// Blue
};

DecodeSignal::DecodeSignal(pv::SigSession &session,
	boost::shared_ptr<pv::data::Decoder> decoder, int index) :
	Trace(session, QString(decoder->get_decoder()->name)),
	_decoder(decoder)
{
	assert(_decoder);

	_colour = DecodeColours[index % countof(DecodeColours)];

	connect(_decoder.get(), SIGNAL(new_decode_data()),
		this, SLOT(on_new_decode_data()));
}

bool DecodeSignal::enabled() const
{
	return true;
}

void DecodeSignal::set_view(pv::view::View *view)
{
	assert(view);
	Trace::set_view(view);
}

void DecodeSignal::paint_back(QPainter &p, int left, int right)
{
	paint_axis(p, get_y(), left, right);
}

void DecodeSignal::paint_mid(QPainter &p, int left, int right)
{
	using namespace pv::view::decode;

	assert(_view);
	const int y = get_y();

	const double scale = _view->scale();
	assert(scale > 0);

	double samplerate = _decoder->get_samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = (_view->offset() -
		_decoder->get_start_time()) / scale;
	const double samples_per_pixel = samplerate * scale;

	assert(_decoder);
	vector< shared_ptr<Annotation> > annotations(_decoder->annotations());
	BOOST_FOREACH(shared_ptr<Annotation> a, annotations) {
		assert(a);
		a->paint(p, get_text_colour(), _text_size.height(),
			left, right, samples_per_pixel, pixels_offset, y);
	}
}

QMenu* DecodeSignal::create_context_menu(QWidget *parent)
{
	QMenu *const menu = Trace::create_context_menu(parent);

	menu->addSeparator();

	QAction *const del = new QAction(tr("Delete"), this);
	connect(del, SIGNAL(triggered()), this, SLOT(on_delete()));
	menu->addAction(del);

	return menu;
}

void DecodeSignal::on_new_decode_data()
{
	if (_view)
		_view->update_viewport();
}

void DecodeSignal::on_delete()
{
	_session.remove_decode_signal(this);
}

} // namespace view
} // namespace pv
