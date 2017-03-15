/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_WIDGETS_IMPORTMENU_HPP
#define PULSEVIEW_PV_WIDGETS_IMPORTMENU_HPP

#include <memory>

#include <QMenu>
#include <QSignalMapper>

using std::shared_ptr;

namespace sigrok {
class Context;
class InputFormat;
}

namespace pv {
namespace widgets {

class ImportMenu : public QMenu
{
	Q_OBJECT;

public:
	ImportMenu(QWidget *parent, shared_ptr<sigrok::Context> context,
		QAction *open_action = nullptr);

private Q_SLOTS:
	void on_action(QObject *action);

Q_SIGNALS:
	void format_selected(shared_ptr<sigrok::InputFormat> format);

private:
	shared_ptr<sigrok::Context> context_;
	QSignalMapper mapper_;
};

} // widgets
} // pv

#endif // PULSEVIEW_PV_WIDGETS_IMPORTMENU_HPP
