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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>

#include <QDebug>
#include <QMessageBox>

#include "pv/session.hpp"

#include "storeprogress.hpp"

using std::map;
using std::pair;
using std::shared_ptr;
using std::string;

using Glib::VariantBase;

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(const QString &file_name,
	const shared_ptr<sigrok::OutputFormat> output_format,
	const map<string, VariantBase> &options,
	const pair<uint64_t, uint64_t> sample_range,
	const Session &session, QWidget *parent) :
	QProgressDialog(tr("Saving..."), tr("Cancel"), 0, 0, parent),
	session_(file_name.toStdString(), output_format, options, sample_range,
		session),
	showing_error_(false)
{
	connect(&session_, SIGNAL(progress_updated()),
		this, SLOT(on_progress_updated()));
	connect(&session_, SIGNAL(store_successful()),
		&session, SLOT(on_data_saved()));
	connect(this, SIGNAL(canceled()), this, SLOT(on_cancel()));

	// Since we're not setting any progress in case of an error, the dialog
	// will pop up after the minimumDuration time has been reached - 4000 ms
	// by default.
	// We do not want this as it overlaps with the error message box, so we
	// set the minimumDuration to 0 so that it only appears when we feed it
	// progress data. Then, call reset() to prevent the progress dialog from
	// popping up anyway. This would happen in Qt5 because the behavior was
	// changed in such a way that the duration timer is started by the
	// constructor. We don't want that and reset() stops the timer, so we
	// use it.
	setMinimumDuration(0);
	reset();
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
	showing_error_ = true;

	qDebug() << "Error trying to save:" << session_.error();

	QMessageBox msg(parentWidget());
	msg.setText(tr("Failed to save session.") + "\n\n" + session_.error());
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();

	close();
}

void StoreProgress::closeEvent(QCloseEvent*)
{
	session_.cancel();

	// Closing doesn't mean we're going to be destroyed because our parent
	// still owns our handle. Make sure this stale instance doesn't hang around.
	deleteLater();
}

void StoreProgress::on_progress_updated()
{
	const pair<int, int> p = session_.progress();
	assert(p.first <= p.second);

	if (p.second) {
		setValue(p.first);
		setMaximum(p.second);
	} else {
		const QString err = session_.error();
		if (!err.isEmpty() && !showing_error_)
			show_error();
	}
}

void StoreProgress::on_cancel()
{
	session_.cancel();
}

}  // namespace dialogs
}  // namespace pv
