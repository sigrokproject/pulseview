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

#include <boost/foreach.hpp>

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>

#include "decodetrace.h"

#include <pv/sigsession.h>
#include <pv/data/decoderstack.h>
#include <pv/view/logicsignal.h>
#include <pv/view/view.h>
#include <pv/view/decode/annotation.h>
#include <pv/widgets/decodermenu.h>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const QColor DecodeTrace::DecodeColours[4] = {
	QColor(0xEF, 0x29, 0x29),	// Red
	QColor(0xFC, 0xE9, 0x4F),	// Yellow
	QColor(0x8A, 0xE2, 0x34),	// Green
	QColor(0x72, 0x9F, 0xCF)	// Blue
};

const QColor DecodeTrace::ErrorBgColour = QColor(0xEF, 0x29, 0x29);

DecodeTrace::DecodeTrace(pv::SigSession &session,
	boost::shared_ptr<pv::data::DecoderStack> decoder_stack, int index) :
	Trace(session, QString(decoder_stack->decoder()->name)),
	_decoder_stack(decoder_stack),
	_binding(decoder_stack)
{
	assert(_decoder_stack);

	_colour = DecodeColours[index % countof(DecodeColours)];

	connect(_decoder_stack.get(), SIGNAL(new_decode_data()),
		this, SLOT(on_new_decode_data()));
}

bool DecodeTrace::enabled() const
{
	return true;
}

const boost::shared_ptr<pv::data::DecoderStack>& DecodeTrace::decoder() const
{
	return _decoder_stack;
}

void DecodeTrace::set_view(pv::view::View *view)
{
	assert(view);
	Trace::set_view(view);
}

void DecodeTrace::paint_back(QPainter &p, int left, int right)
{
	paint_axis(p, get_y(), left, right);
}

void DecodeTrace::paint_mid(QPainter &p, int left, int right)
{
	using namespace pv::view::decode;

	assert(_decoder_stack);
	const QString err = _decoder_stack->error_message();
	if (!err.isEmpty()) {
		draw_error(p, err, left, right);
		return;
	}

	assert(_view);
	const int y = get_y();

	const double scale = _view->scale();
	assert(scale > 0);

	double samplerate = _decoder_stack->get_samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = (_view->offset() -
		_decoder_stack->get_start_time()) / scale;
	const double samples_per_pixel = samplerate * scale;

	assert(_decoder_stack);
	vector< shared_ptr<Annotation> > annotations(_decoder_stack->annotations());
	BOOST_FOREACH(shared_ptr<Annotation> a, annotations) {
		assert(a);
		a->paint(p, get_text_colour(), _text_size.height(),
			left, right, samples_per_pixel, pixels_offset, y);
	}
}

void DecodeTrace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	const GSList *probe;

	assert(form);
	assert(parent);
	assert(_decoder_stack);

	const srd_decoder *const decoder = _decoder_stack->decoder();

	assert(decoder);

	Trace::populate_popup_form(parent, form);

	form->addRow(new QLabel(tr("<h3>Probes</h3>"), parent));

	_probe_selector_map.clear();

	// Add the mandatory probes
	for(probe = decoder->probes; probe; probe = probe->next) {
		const struct srd_probe *const p =
			(struct srd_probe *)probe->data;
		QComboBox *const combo = create_probe_selector(parent, p);
		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_probe_selected(int)));
		form->addRow(tr("<b>%1</b> (%2) *")
			.arg(p->name).arg(p->desc), combo);

		_probe_selector_map[p] = combo;
	}

	// Add the optional probes
	for(probe = decoder->opt_probes; probe; probe = probe->next) {
		const struct srd_probe *const p =
			(struct srd_probe *)probe->data;
		QComboBox *const combo = create_probe_selector(parent, p);
		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_probe_selected(int)));
		form->addRow(tr("<b>%1</b> (%2)")
			.arg(p->name).arg(p->desc), combo);

		_probe_selector_map[p] = combo;
	}

	form->addRow(new QLabel(
		tr("<i>* Required Probes</i>"), parent));

	// Add the options
	if (!_binding.properties().empty()) {
		form->addRow(new QLabel(tr("<h3>Options</h3>"),
			parent));
		_binding.add_properties_to_form(form, true);
	}

	// Add stacking button
	QPushButton *const stack_button =
		new QPushButton(tr("Stack DecoderStack"), parent);
	pv::widgets::DecoderMenu *const decoder_menu =
		new pv::widgets::DecoderMenu(parent);
	stack_button->setMenu(decoder_menu);

	QHBoxLayout *stack_button_box = new QHBoxLayout;
	stack_button_box->addWidget(stack_button, 0, Qt::AlignRight);
	form->addRow(stack_button_box);
}

QMenu* DecodeTrace::create_context_menu(QWidget *parent)
{
	QMenu *const menu = Trace::create_context_menu(parent);

	menu->addSeparator();

	QAction *const del = new QAction(tr("Delete"), this);
	del->setShortcuts(QKeySequence::Delete);
	connect(del, SIGNAL(triggered()), this, SLOT(on_delete()));
	menu->addAction(del);

	return menu;
}

void DecodeTrace::draw_error(QPainter &p, const QString &message,
	int left, int right)
{
	const int y = get_y();

	p.setPen(ErrorBgColour.darker());
	p.setBrush(ErrorBgColour);

	const QRectF bounding_rect =
		QRectF(left, INT_MIN / 2 + y, right - left, INT_MAX);
	const QRectF text_rect = p.boundingRect(bounding_rect,
		Qt::AlignCenter, message);
	const float r = text_rect.height() / 4;

	p.drawRoundedRect(text_rect.adjusted(-r, -r, r, r), r, r,
		Qt::AbsoluteSize);

	p.setPen(get_text_colour());
	p.drawText(text_rect, message);
}

QComboBox* DecodeTrace::create_probe_selector(
	QWidget *parent, const srd_probe *const probe)
{
	const vector< shared_ptr<Signal> > sigs = _session.get_signals();

	assert(_decoder_stack);
	const map<const srd_probe*,
		shared_ptr<LogicSignal> >::const_iterator probe_iter =
		_decoder_stack->probes().find(probe);

	QComboBox *selector = new QComboBox(parent);

	selector->addItem("-", qVariantFromValue((void*)NULL));

	if (probe_iter == _decoder_stack->probes().end())
		selector->setCurrentIndex(0);

	for(size_t i = 0; i < sigs.size(); i++) {
		const shared_ptr<view::Signal> s(sigs[i]);
		assert(s);

		if (dynamic_pointer_cast<LogicSignal>(s) && s->enabled())
		{
			selector->addItem(s->get_name(),
				qVariantFromValue((void*)s.get()));
			if ((*probe_iter).second == s)
				selector->setCurrentIndex(i + 1);
		}
	}

	return selector;
}

void DecodeTrace::commit_probes()
{
	assert(_decoder_stack);

	map<const srd_probe*, shared_ptr<LogicSignal> > probe_map;
	const vector< shared_ptr<Signal> > sigs = _session.get_signals();

	for(map<const srd_probe*, QComboBox*>::const_iterator i =
		_probe_selector_map.begin();
		i != _probe_selector_map.end(); i++)
	{
		const QComboBox *const combo = (*i).second;
		const LogicSignal *const selection =
			(LogicSignal*)combo->itemData(combo->currentIndex()).
			value<void*>();

		BOOST_FOREACH(shared_ptr<Signal> s, sigs)
			if(s.get() == selection) {
				probe_map[(*i).first] =
					dynamic_pointer_cast<LogicSignal>(s);
				break;
			}
	}

	_decoder_stack->set_probes(probe_map);
}

void DecodeTrace::on_new_decode_data()
{
	if (_view)
		_view->update_viewport();
}

void DecodeTrace::delete_pressed()
{
	on_delete();
}

void DecodeTrace::on_delete()
{
	_session.remove_decode_signal(this);
}

void DecodeTrace::on_probe_selected(int)
{
	commit_probes();
}

} // namespace view
} // namespace pv
