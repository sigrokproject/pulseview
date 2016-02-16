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

#include <assert.h>

#include <QCheckBox>

#include "bool.hpp"

namespace pv {
namespace prop {

Bool::Bool(QString name, Getter getter, Setter setter) :
	Property(name, getter, setter),
	check_box_(nullptr)
{
}

QWidget* Bool::get_widget(QWidget *parent, bool auto_commit)
{
	if (check_box_)
		return check_box_;

	if (!getter_)
		return nullptr;

	Glib::VariantBase variant = getter_();
	if (!variant.gobj())
		return nullptr;

	bool value = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(
		variant).get();

	check_box_ = new QCheckBox(name(), parent);
	check_box_->setCheckState(value ? Qt::Checked : Qt::Unchecked);

	if (auto_commit)
		connect(check_box_, SIGNAL(stateChanged(int)),
			this, SLOT(on_state_changed(int)));

	return check_box_;
}

bool Bool::labeled_widget() const
{
	return true;
}

void Bool::commit()
{
	assert(setter_);

	if (!check_box_)
		return;

	setter_(Glib::Variant<bool>::create(
		check_box_->checkState() == Qt::Checked));
}

void Bool::on_state_changed(int)
{
	commit();
}

} // prop
} // pv
