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

#include <cassert>

#include <QDebug>
#include <QLineEdit>
#include <QSpinBox>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "string.hpp"

using std::string;

using Glib::ustring;

namespace pv {
namespace prop {

String::String(QString name,
	QString desc,
	Getter getter,
	Setter setter) :
	Property(name, desc, getter, setter),
	line_edit_(nullptr)
{
}

QWidget* String::get_widget(QWidget *parent, bool auto_commit)
{
	if (line_edit_)
		return line_edit_;

	if (!getter_)
		return nullptr;

	try {
		Glib::VariantBase variant = getter_();
		if (!variant.gobj())
			return nullptr;
	} catch (const sigrok::Error &e) {
		qWarning() << tr("Querying config key %1 resulted in %2").arg(name_, e.what());
		return nullptr;
	}

	line_edit_ = new QLineEdit(parent);

	update_widget();

	if (auto_commit)
		connect(line_edit_, SIGNAL(textEdited(const QString&)),
			this, SLOT(on_text_edited(const QString&)));

	return line_edit_;
}

void String::update_widget()
{
	if (!line_edit_)
		return;

	Glib::VariantBase variant;

	try {
		variant = getter_();
	} catch (const sigrok::Error &e) {
		qWarning() << tr("Querying config key %1 resulted in %2").arg(name_, e.what());
		return;
	}

	assert(variant.gobj());

	string value = Glib::VariantBase::cast_dynamic<Glib::Variant<ustring>>(
		variant).get();

	line_edit_->setText(QString::fromStdString(value));
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

}  // namespace prop
}  // namespace pv
