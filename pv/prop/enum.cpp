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
#include <limits>
#include <vector>

#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

#include "enum.hpp"

using std::abs;
// Note that "using std::isnan;" is _not_ put here since that would break
// compilation on some platforms. Use "std::isnan()" instead in checks below.
using std::numeric_limits;
using std::pair;
using std::vector;

namespace pv {
namespace prop {

Enum::Enum(QString name, QString desc,
	vector<pair<Glib::VariantBase, QString> > values,
	Getter getter, Setter setter) :
	Property(name, desc, getter, setter),
	values_(values),
	is_range_(false),
	selector_(nullptr),
	slider_layout_widget_(nullptr),
	slider_(nullptr),
	slider_label_(nullptr)
{
	// Try to determine whether the values make up a range, created by e.g.
	// std_gvar_min_max_step_thresholds()

	vector<double> deltas;
	double prev_value = 0;

	for (const pair<Glib::VariantBase, QString> &v : values_) {
		gdouble value;
		if (v.first.is_of_type(Glib::VariantType("d"))) {
			g_variant_get((GVariant*)(v.first.gobj()), "d", &value);
		} else if (v.first.is_of_type(Glib::VariantType("(dd)"))) {
			gdouble dummy;
			g_variant_get((GVariant*)(v.first.gobj()), "(dd)", &value, &dummy);
		} else
			break; // Type not d or (dd), so not a range that we can handle
		deltas.push_back(value - prev_value);
		prev_value = value;
	}

	if (deltas.size() > 0) {
		bool constant_delta = true;
		double prev_delta = numeric_limits<double>::quiet_NaN();

		bool skip_first = true;
		for (double delta : deltas) {
			// First value is incorrect, it's the delta to 0 since no
			// previous value existed yet
			if (skip_first) {
				skip_first = false;
				continue;
			}
			if (std::isnan(prev_delta))
				prev_delta = delta;

			// 2*DBL_EPSILON doesn't work here, so use a workaround
			if (abs(delta - prev_delta) > (delta/10))
				constant_delta = false;

			prev_delta = delta;
		}

		if (constant_delta)
			is_range_ = true;
	}
}

QWidget* Enum::get_widget(QWidget *parent, bool auto_commit)
{
	if (!getter_)
		return nullptr;

	Glib::VariantBase variant = getter_();
	if (!variant.gobj())
		return nullptr;

	if (is_range_) {
		// Use slider
		if (slider_layout_widget_)
			return slider_layout_widget_;

		slider_ = new QSlider();
		// Sliders can't handle float values, so we just use it to specify
		// the number of steps that we're away from the range's beginning
		slider_->setOrientation(Qt::Horizontal);
		slider_->setMinimum(0);
		slider_->setMaximum(values_.size() - 1);
		slider_->setSingleStep(1);

		slider_label_ = new QLabel();

		slider_layout_widget_ = new QWidget(parent);
		QHBoxLayout *layout = new QHBoxLayout(slider_layout_widget_);
		layout->addWidget(slider_);
		layout->addWidget(slider_label_);

		update_widget();

		if (auto_commit)
			connect(slider_, SIGNAL(valueChanged(int)),
				this, SLOT(on_value_changed(int)));

		return slider_layout_widget_;

	} else {
		// Use combo box
		if (selector_)
			return selector_;

		selector_ = new QComboBox(parent);
		for (unsigned int i = 0; i < values_.size(); i++) {
			const pair<Glib::VariantBase, QString> &v = values_[i];
			selector_->addItem(v.second, qVariantFromValue(v.first));
		}

		update_widget();

		if (auto_commit)
			connect(selector_, SIGNAL(currentIndexChanged(int)),
				this, SLOT(on_current_index_changed(int)));

		return selector_;
	}
}

void Enum::update_widget()
{
	Glib::VariantBase variant = getter_();
	assert(variant.gobj());

	if (is_range_) {

		// Use slider
		if (!slider_layout_widget_)
			return;

		for (unsigned int i = 0; i < values_.size(); i++) {
			const pair<Glib::VariantBase, QString> &v = values_[i];

			// g_variant_equal() doesn't handle floating point properly
			if (v.first.is_of_type(Glib::VariantType("d"))) {
				gdouble a, b;
				g_variant_get(variant.gobj(), "d", &a);
				g_variant_get((GVariant*)(v.first.gobj()), "d", &b);

				if (abs(a - b) <= 2 * DBL_EPSILON) {
					slider_->setValue(i);
					slider_label_->setText(v.second);
				}
			} else {
				// Check for "(dd)" type and handle it if it's found
				if (v.first.is_of_type(Glib::VariantType("(dd)"))) {
					gdouble a1, a2, b1, b2;
					g_variant_get(variant.gobj(), "(dd)", &a1, &a2);
					g_variant_get((GVariant*)(v.first.gobj()), "(dd)", &b1, &b2);

					if ((abs(a1 - b1) <= 2 * DBL_EPSILON) && \
						(abs(a2 - b2) <= 2 * DBL_EPSILON)) {
						slider_->setValue(i);
						slider_label_->setText(v.second);
					}

				} else {
					qWarning() << "Enum property" << name() << "encountered unsupported type";
					return;
				}
			}
		}

	} else {
		// Use combo box
		if (!selector_)
			return;

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
}

void Enum::commit()
{
	assert(setter_);

	if (is_range_) {
		// Use slider
		if (!slider_layout_widget_)
			return;

		setter_(values_.at(slider_->value()).first);

		update_widget();
	} else {
		// Use combo box
		if (!selector_)
			return;

		const int index = selector_->currentIndex();
		if (index < 0)
			return;

		setter_(selector_->itemData(index).value<Glib::VariantBase>());

		// The combo box needs no update, it already shows the current value
		// by definition: the user picked it
	}
}

void Enum::on_current_index_changed(int)
{
	commit();
}

void Enum::on_value_changed(int)
{
	commit();
}

}  // namespace prop
}  // namespace pv
