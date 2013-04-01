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

#include <glib-2.0/glib.h>
#include <QComboBox>

#include "enum.h"

using namespace boost;
using namespace std;

namespace pv {
namespace prop {

Enum::Enum(QString name,
	vector<pair<const void*, QString> > values,
	function<GVariant* ()> getter,
	function<void (GVariant*)> setter) :
	Property(name),
	_values(values),
	_getter(getter),
	_setter(setter),
	_selector(NULL)
{
}

QWidget* Enum::get_widget(QWidget *parent)
{
	if (_selector)
		return _selector;

	const void *value = NULL;
	if (_getter)
		value = _getter();

	_selector = new QComboBox(parent);
	for (unsigned int i = 0; i < _values.size(); i++) {
		const pair<const void*, QString> &v = _values[i];
		_selector->addItem(v.second,
			qVariantFromValue((void*)v.first));
		if (v.first == value)
			_selector->setCurrentIndex(i);
	}

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

	_setter(_selector->itemData(index).value<GVariant*>());
}

} // prop
} // pv
