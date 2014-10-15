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

#include "enum.h"

using std::pair;
using std::vector;

namespace pv {
namespace prop {

Enum::Enum(QString name,
	vector<pair<Glib::VariantBase, QString> > values,
	Getter getter, Setter setter) :
	Property(name, getter, setter),
	_values(values),
	_selector(NULL)
{
}

Enum::~Enum()
{
}

QWidget* Enum::get_widget(QWidget *parent, bool auto_commit)
{
	if (_selector)
		return _selector;

	if (!_getter)
		return NULL;

	Glib::VariantBase variant = _getter();
	if (!variant.gobj())
		return NULL;

	_selector = new QComboBox(parent);
	for (unsigned int i = 0; i < _values.size(); i++) {
		const pair<Glib::VariantBase, QString> &v = _values[i];
		_selector->addItem(v.second, qVariantFromValue(v.first));
		if (v.first.equal(variant))
			_selector->setCurrentIndex(i);
	}

	if (auto_commit)
		connect(_selector, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_current_item_changed(int)));

	return _selector;
}

void Enum::commit()
{
	assert(_setter);

	if (!_selector)
		return;

	const int index = _selector->currentIndex();
	if (index < 0)
		return;

	_setter(_selector->itemData(index).value<Glib::VariantBase>());
}

void Enum::on_current_item_changed(int)
{
	commit();
}

} // prop
} // pv
