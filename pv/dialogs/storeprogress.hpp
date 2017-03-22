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

#ifndef PULSEVIEW_PV_DIALOGS_SAVEPROGRESS_HPP
#define PULSEVIEW_PV_DIALOGS_SAVEPROGRESS_HPP

#include <memory>
#include <set>

#include <QProgressDialog>

#include <pv/storesession.hpp>

using std::map;
using std::pair;
using std::shared_ptr;
using std::string;

namespace pv {

class Session;

namespace dialogs {

class StoreProgress : public QProgressDialog
{
	Q_OBJECT

public:
	StoreProgress(const QString &file_name,
		const shared_ptr<sigrok::OutputFormat> output_format,
		const map<string, Glib::VariantBase> &options,
		const pair<uint64_t, uint64_t> sample_range,
		const Session &session, QWidget *parent = nullptr);

	virtual ~StoreProgress();

	void run();

private:
	void show_error();

	void closeEvent(QCloseEvent*);

private Q_SLOTS:
	void on_progress_updated();

private:
	pv::StoreSession session_;
};

}  // namespace dialogs
}  // namespace pv

#endif // PULSEVIEW_PV_DIALOGS_SAVEPROGRESS_HPP
