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

using namespace std;

namespace pv {
namespace prop {

Enum::Enum(QString name,
	std::vector<std::pair<const void*, QString> > values,
	boost::function<const void* ()> getter,
	boost::function<void (const void*)> setter) :
	Property(name),
	_values(values),
	_getter(getter),
	_setter(setter),
	_selector(NULL)
{
}

QWidget* Enum::get_widget(QWidget *parent)
{
	if(_selector)
		return _selector;

	_selector = new QComboBox(parent);
	for(vector< pair<const void*, QString> >::const_iterator i =
			_values.begin();
		i != _values.end(); i++)
		_selector->addItem((*i).second,
			qVariantFromValue((void*)(*i).first));

	return _selector;
}

void Enum::commit()
{
	assert(_setter);

	if(!_selector)
		return;

	const int index = _selector->currentIndex();
	if (index < 0)
		return;

	_setter(_selector->itemData(index).value<void*>());
}

} // prop
} // pv
