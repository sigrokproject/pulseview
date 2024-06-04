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

#include <cassert>

#include <libsigrokdecode/libsigrokdecode.h>

#include "decodermenu.hpp"

namespace pv {
namespace widgets {

DecoderMenu::DecoderMenu(QWidget *parent, const char* input, bool first_level_decoder) :
	QMenu(parent),
	mapper_(this)
{
	GSList *li = g_slist_sort(g_slist_copy((GSList*)srd_decoder_list()), decoder_name_cmp);

	for (GSList *l = li; l; l = l->next) {
		const srd_decoder *const d = (srd_decoder*)l->data;
		assert(d);

		const bool have_channels = (d->channels || d->opt_channels) != 0;
		if (first_level_decoder != have_channels)
			continue;

		if (!first_level_decoder) {
			// Dismiss all non-stacked decoders unless we're looking for first-level decoders
			if (!d->inputs)
				continue;

			// TODO For now we ignore that d->inputs is actually a list
			if (strncmp((char*)(d->inputs->data), input, 1024) != 0)
				continue;
		}

		QAction *const action = addAction(QString::fromUtf8(d->name));
		action->setData(QVariant::fromValue(l->data));
		mapper_.setMapping(action, action);
		connect(action, SIGNAL(triggered()), &mapper_, SLOT(map()));
	}
	g_slist_free(li);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	connect(&mapper_, SIGNAL(mappedObject(QObject*)), this, SLOT(on_action(QObject*)));
#else
	connect(&mapper_, SIGNAL(mapped(QObject*)), this, SLOT(on_action(QObject*)));
#endif
}

int DecoderMenu::decoder_name_cmp(const void *a, const void *b)
{
	return strcmp(((const srd_decoder*)a)->name, ((const srd_decoder*)b)->name);
}

void DecoderMenu::on_action(QObject *action)
{
	assert(action);

	srd_decoder *const dec = (srd_decoder*)((QAction*)action)->data().value<void*>();
	assert(dec);

	decoder_selected(dec);
}

}  // namespace widgets
}  // namespace pv
