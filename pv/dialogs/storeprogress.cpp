/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <QMessageBox>

#include "storeprogress.hpp"

using std::map;
using std::string;

using Glib::VariantBase;

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(const QString &file_name,
	const std::shared_ptr<sigrok::OutputFormat> output_format,
	const map<string, VariantBase> &options,
	const std::pair<uint64_t, uint64_t> sample_range,
	const Session &session, QWidget *parent) :
	QProgressDialog(tr("Saving..."), tr("Cancel"), 0, 0, parent),
	session_(file_name.toStdString(), output_format, options, sample_range,
		session)
{
	connect(&session_, SIGNAL(progress_updated()),
		this, SLOT(on_progress_updated()));
}

StoreProgress::~StoreProgress()
{
	session_.wait();
}

void StoreProgress::run()
{
	if (session_.start())
		show();
	else
		show_error();
}

void StoreProgress::show_error()
{
	QMessageBox msg(parentWidget());
	msg.setText(tr("Failed to save session."));
	msg.setInformativeText(session_.error());
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void StoreProgress::closeEvent(QCloseEvent*)
{
	session_.cancel();
}

void StoreProgress::on_progress_updated()
{
	const std::pair<int, int> p = session_.progress();
	assert(p.first <= p.second);

	if (p.second) {
		setValue(p.first);
		setMaximum(p.second);
	} else {
		const QString err = session_.error();
		if (!err.isEmpty())
			show_error();
		close();
	}
}

} // dialogs
} // pv
