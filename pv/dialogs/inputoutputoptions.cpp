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

#include "inputoutputoptions.hpp"

#include <pv/prop/property.hpp>

using std::map;
using std::shared_ptr;
using std::string;

using Glib::VariantBase;

using sigrok::Option;

namespace pv {
namespace dialogs {

InputOutputOptions::InputOutputOptions(const QString &title,
	const map<string, shared_ptr<Option>> &options, QWidget *parent) :
	QDialog(parent),
	layout_(this),
	button_box_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this),
	binding_(options)
{
	setWindowTitle(title);

	connect(&button_box_, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&button_box_, SIGNAL(rejected()), this, SLOT(reject()));

	setLayout(&layout_);

	layout_.addWidget(binding_.get_property_form(this));
	layout_.addWidget(&button_box_);
}

const map<string, VariantBase>& InputOutputOptions::options() const
{
	return binding_.options();
}

void InputOutputOptions::accept()
{
	QDialog::accept();

	// Commit the properties
	binding_.commit();
}

} // namespace dialogs
} // namespace pv
