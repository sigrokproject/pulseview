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

#include <assert.h>

#include <QComboBox>

#include "enum.hpp"

using std::pair;
using std::vector;

namespace pv {
namespace prop {

Enum::Enum(QString name,
	vector<pair<Glib::VariantBase, QString> > values,
	Getter getter, Setter setter) :
	Property(name, getter, setter),
	values_(values),
	selector_(nullptr)
{
}

QWidget* Enum::get_widget(QWidget *parent, bool auto_commit)
{
	if (selector_)
		return selector_;

	if (!getter_)
		return nullptr;

	Glib::VariantBase variant = getter_();
	if (!variant.gobj())
		return nullptr;

	selector_ = new QComboBox(parent);
	for (unsigned int i = 0; i < values_.size(); i++) {
		const pair<Glib::VariantBase, QString> &v = values_[i];
		selector_->addItem(v.second, qVariantFromValue(v.first));
		if (v.first.equal(variant))
			selector_->setCurrentIndex(i);
	}

	if (auto_commit)
		connect(selector_, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_current_item_changed(int)));

	return selector_;
}

void Enum::commit()
{
	assert(setter_);

	if (!selector_)
		return;

	const int index = selector_->currentIndex();
	if (index < 0)
		return;

	setter_(selector_->itemData(index).value<Glib::VariantBase>());
}

void Enum::on_current_item_changed(int)
{
	commit();
}

} // prop
} // pv
