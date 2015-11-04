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

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "exportmenu.hpp"

using std::find_if;
using std::map;
using std::pair;
using std::string;
using std::shared_ptr;

using sigrok::Context;
using sigrok::OutputFormat;

namespace pv {
namespace widgets {

ExportMenu::ExportMenu(QWidget *parent, shared_ptr<Context> context,
	std::vector<QAction *>open_actions) :
	QMenu(parent),
	context_(context),
	mapper_(this)
{
	assert(context);

	if (!open_actions.empty()) {
		bool first_action = true;
		for (auto open_action : open_actions) {
			addAction(open_action);

			if (first_action) {
				first_action = false;
				setDefaultAction(open_action);
			}
		}
		addSeparator();
	}

	const map<string, shared_ptr<OutputFormat> > formats =
		context->output_formats();

	for (const pair<string, shared_ptr<OutputFormat> > &f : formats) {
		if (f.first == "srzip")
			continue;

		assert(f.second);
		QAction *const action =	addAction(tr("Export %1...")
			.arg(QString::fromStdString(f.second->description())));
		action->setData(qVariantFromValue((void*)f.second.get()));
		mapper_.setMapping(action, action);
		connect(action, SIGNAL(triggered()), &mapper_, SLOT(map()));
	}

	connect(&mapper_, SIGNAL(mapped(QObject*)),
		this, SLOT(on_action(QObject*)));
}

void ExportMenu::on_action(QObject *action)
{
	assert(action);

	const map<string, shared_ptr<OutputFormat> > formats =
		context_->output_formats();
	const auto iter = find_if(formats.cbegin(), formats.cend(),
		[&](const pair<string, shared_ptr<OutputFormat> > &f) {
			return f.second.get() ==
				((QAction*)action)->data().value<void*>(); });
	if (iter == formats.cend())
		return;

	format_selected((*iter).second);
}

} // widgets
} // pv
