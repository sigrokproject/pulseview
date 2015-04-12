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

#include <QLineEdit>
#include <QSpinBox>

#include "string.hpp"

using std::string;

using Glib::ustring;

namespace pv {
namespace prop {

String::String(QString name,
	Getter getter,
	Setter setter) :
	Property(name, getter, setter),
	line_edit_(nullptr)
{
}

QWidget* String::get_widget(QWidget *parent, bool auto_commit)
{
	if (line_edit_)
		return line_edit_;

	if (!getter_)
		return nullptr;

	Glib::VariantBase variant = getter_();
	if (!variant.gobj())
		return nullptr;

	string value = Glib::VariantBase::cast_dynamic<Glib::Variant<ustring>>(
		variant).get();

	line_edit_ = new QLineEdit(parent);
	line_edit_->setText(QString::fromStdString(value));

	if (auto_commit)
		connect(line_edit_, SIGNAL(textEdited(const QString&)),
			this, SLOT(on_text_edited(const QString&)));

	return line_edit_;
}

void String::commit()
{
	assert(setter_);

	if (!line_edit_)
		return;

	QByteArray ba = line_edit_->text().toLocal8Bit();
	setter_(Glib::Variant<ustring>::create(ba.data()));
}

void String::on_text_edited(const QString&)
{
	commit();
}

} // prop
} // pv
