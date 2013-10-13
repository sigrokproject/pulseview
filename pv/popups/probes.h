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

#ifndef PULSEVIEW_PV_POPUPS_PROBES_H
#define PULSEVIEW_PV_POPUPS_PROBES_H

#include <QListWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <pv/widgets/popup.h>

namespace pv {

class SigSession;

namespace popups {

class Probes : public pv::widgets::Popup
{
	Q_OBJECT

public:
	Probes(SigSession &_session, QWidget *parent);

private:
	void set_all_probes(bool set);

private:
	void showEvent(QShowEvent *e);

private slots:
	void item_changed(QListWidgetItem *item);

	void enable_all_probes();
	void disable_all_probes();

private:
	pv::SigSession &_session;

	QVBoxLayout _layout;

	QListWidget _probes;
	bool _updating_probes;

	QToolBar _probes_bar;
	QToolButton _enable_all_probes;
	QToolButton _disable_all_probes;
};

} // popups
} // pv

#endif // PULSEVIEW_PV_POPUPS_PROBES_H
