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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>
#include <cfloat>
#include <cmath>

#include <QComboBox>

#include "enum.hpp"

using std::abs;
using std::pair;
using std::vector;

namespace pv {
namespace prop {

Enum::Enum(QString name, QString desc,
	vector<pair<Glib::VariantBase, QString> > values,
	Getter getter, Setter setter) :
	Property(name, desc, getter, setter),
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
	}

	update_widget();

	if (auto_commit)
		connect(selector_, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_current_item_changed(int)));

	return selector_;
}

void Enum::update_widget()
{
	if (!selector_)
		return;

	Glib::VariantBase variant = getter_();
	assert(variant.gobj());

	for (unsigned int i = 0; i < values_.size(); i++) {
		const pair<Glib::VariantBase, QString> &v = values_[i];

		// g_variant_equal() doesn't handle floating point properly
		if (v.first.is_of_type(Glib::VariantType("d"))) {
			gdouble a, b;
			g_variant_get(variant.gobj(), "d", &a);
			g_variant_get((GVariant*)(v.first.gobj()), "d", &b);
			if (abs(a - b) <= 2 * DBL_EPSILON)
				selector_->setCurrentIndex(i);
		} else {
			// Check for "(dd)" type and handle it if it's found
			if (v.first.is_of_type(Glib::VariantType("(dd)"))) {
				gdouble a1, a2, b1, b2;
				g_variant_get(variant.gobj(), "(dd)", &a1, &a2);
				g_variant_get((GVariant*)(v.first.gobj()), "(dd)", &b1, &b2);
				if ((abs(a1 - b1) <= 2 * DBL_EPSILON) && \
					(abs(a2 - b2) <= 2 * DBL_EPSILON))
					selector_->setCurrentIndex(i);

			} else
				// Handle all other types
				if (v.first.equal(variant))
					selector_->setCurrentIndex(i);
		}
	}
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

}  // namespace prop
}  // namespace pv
