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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <cassert>

#include <libsigrokdecode/libsigrokdecode.h>

#include "decodermenu.hpp"

namespace pv {
namespace widgets {

DecoderMenu::DecoderMenu(QWidget *parent, bool first_level_decoder) :
	QMenu(parent),
	mapper_(this)
{
	GSList *l = g_slist_sort(g_slist_copy(
		(GSList*)srd_decoder_list()), decoder_name_cmp);
	for (; l; l = l->next) {
		const srd_decoder *const d = (srd_decoder*)l->data;
		assert(d);

		const bool have_channels = (d->channels || d->opt_channels) != 0;
		if (first_level_decoder == have_channels) {
			QAction *const action =
				addAction(QString::fromUtf8(d->name));
			action->setData(qVariantFromValue(l->data));
			mapper_.setMapping(action, action);
			connect(action, SIGNAL(triggered()),
				&mapper_, SLOT(map()));
		}
	}
	g_slist_free(l);

	connect(&mapper_, SIGNAL(mapped(QObject*)),
		this, SLOT(on_action(QObject*)));
}

int DecoderMenu::decoder_name_cmp(const void *a, const void *b)
{
	return strcmp(((const srd_decoder*)a)->name,
		((const srd_decoder*)b)->name);
}

void DecoderMenu::on_action(QObject *action)
{
	assert(action);
	srd_decoder *const dec =
		(srd_decoder*)((QAction*)action)->data().value<void*>();
	assert(dec);

	decoder_selected(dec);	
}

} // widgets
} // pv
