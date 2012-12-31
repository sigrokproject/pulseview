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

#include "decoder.h"

#include <pv/view/signal.h>

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

Decoder::Decoder(QWidget *parent, const srd_decoder *decoder,
	const vector< shared_ptr<view::Signal> > &sigs) :
	QDialog(parent),
	_decoder(decoder),
	_sigs(sigs),
	_layout(this),
	_form(this),
	_form_layout(&_form),
	_heading(this),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this)
{
	const GSList *probe;

	setWindowTitle(tr("Configure %1").arg(decoder->name));

	_heading.setText(tr("<h2>%1</h2>%2")
		.arg(decoder->longname)
		.arg(decoder->desc));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	_form.setLayout(&_form_layout);

	setLayout(&_layout);
	_layout.addWidget(&_heading);
	_layout.addWidget(&_form);
	_layout.addWidget(&_button_box);

	_form_layout.addRow(new QLabel("<h3>Probes</h3>", &_form));

	// Add the mandatory probes
	for(probe = decoder->probes; probe; probe = probe->next) {
		const struct srd_probe *const p =
			(struct srd_probe *)probe->data;
		QComboBox *const combo = create_probe_selector(
			&_form, p->name);
		_form_layout.addRow(tr("<b>%1</b> (%2) *")
			.arg(p->name).arg(p->desc), combo);

		_probe_selector_map[p] = combo;
	}

	// Add the optional probes
	for(probe = decoder->opt_probes; probe; probe = probe->next) {
		const struct srd_probe *const p =
			(struct srd_probe *)probe->data;
		QComboBox *const combo = create_probe_selector(
			&_form, p->name);
		_form_layout.addRow(tr("<b>%1</b> (%2)")
			.arg(p->name).arg(p->desc), combo);

		_probe_selector_map[p] = combo;
	}
}

QComboBox* Decoder::create_probe_selector(
	QWidget *parent, const char *name)
{
	QComboBox *selector = new QComboBox(parent);

	selector->addItem("-", qVariantFromValue(-1));
	selector->setCurrentIndex(0);

	for(size_t i = 0; i < _sigs.size(); i++) {
		const shared_ptr<view::Signal> s(_sigs[i]);
		assert(s);

		if (s->enabled()) {
			selector->addItem(s->get_name(), qVariantFromValue(i));
			if(s->get_name().toLower().contains(
				QString(name).toLower()))
				selector->setCurrentIndex(i + 1);
		}
	}

	return selector;
}

} // namespace dialogs
} // namespace pv
