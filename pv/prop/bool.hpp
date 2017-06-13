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

#ifndef PULSEVIEW_PV_PROP_BOOL_HPP
#define PULSEVIEW_PV_PROP_BOOL_HPP

#include "property.hpp"

class QCheckBox;

namespace pv {
namespace prop {

class Bool : public Property
{
	Q_OBJECT;

public:
	Bool(QString name, QString desc, Getter getter, Setter setter);

	virtual ~Bool() = default;

	QWidget* get_widget(QWidget *parent, bool auto_commit);
	bool labeled_widget() const;
	void update_widget();

	void commit();

private Q_SLOTS:
	void on_state_changed(int);

private:
	QCheckBox *check_box_;
};

}  // namespace prop
}  // namespace pv

#endif // PULSEVIEW_PV_PROP_BOOL_HPP
