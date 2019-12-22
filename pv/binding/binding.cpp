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

#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>

#include <pv/prop/property.hpp>

#include "binding.hpp"

using std::shared_ptr;
using std::string;
using std::vector;

namespace pv {
namespace binding {

const vector< shared_ptr<prop::Property> >& Binding::properties()
{
	return properties_;
}

void Binding::commit()
{
	for (shared_ptr<pv::prop::Property> p : properties_) {
		assert(p);
		p->commit();
	}
}

void Binding::add_properties_to_form(QFormLayout *layout, bool auto_commit)
{
	assert(layout);

	help_labels_.clear();

	for (shared_ptr<pv::prop::Property> p : properties_) {
		assert(p);

		QWidget *widget;
		QLabel *help_lbl = nullptr;

		if (p->desc().isEmpty()) {
			widget = p->get_widget(layout->parentWidget(), auto_commit);
		} else {
			QPushButton *help_btn = new QPushButton();
			help_btn->setFlat(true);
			help_btn->setIcon(QIcon(":/icons/help-browser.png"));
			help_btn->setToolTip(p->desc());
			connect(help_btn, SIGNAL(clicked(bool)),
				this, SLOT(on_help_clicked()));

			QHBoxLayout *layout = new QHBoxLayout();
			layout->setContentsMargins(0, 0, 0, 0);
			layout->addWidget(p->get_widget(layout->parentWidget(), auto_commit));
			layout->addWidget(help_btn, 0, Qt::AlignRight);

			widget = new QWidget();
			widget->setLayout(layout);

			help_lbl = new QLabel(p->desc());
			help_lbl->setVisible(false);
			help_lbl->setWordWrap(true);
			help_lbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
			help_labels_[help_btn] = help_lbl;
		}

		if (p->labeled_widget()) {
			layout->addRow(widget);
		} else {
			auto *lbl = new QLabel(p->name());
			lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			layout->addRow(lbl, widget);
		}

		if (help_lbl)
			layout->addRow(help_lbl);
	}
}

QWidget* Binding::get_property_form(QWidget *parent, bool auto_commit)
{
	QWidget *const form = new QWidget(parent);
	QFormLayout *const layout = new QFormLayout(form);
	form->setLayout(layout);
	add_properties_to_form(layout, auto_commit);
	return form;
}

void Binding::update_property_widgets()
{
	for (shared_ptr<pv::prop::Property> p : properties_) {
		assert(p);
		p->update_widget();
	}
}

QString Binding::print_gvariant(Glib::VariantBase gvar)
{
	QString s;

	if (!gvar.gobj())
		s = QString::fromStdString("(null)");
	else if (gvar.is_of_type(Glib::VariantType("s")))
		s = QString::fromStdString(
			Glib::VariantBase::cast_dynamic<Glib::Variant<string>>(gvar).get());
	else
		s = QString::fromStdString(gvar.print());

	return s;
}

void Binding::on_help_clicked()
{
	QPushButton *btn = qobject_cast<QPushButton*>(QObject::sender());
	assert(btn);

	QLabel *lbl = help_labels_.at(btn);
	lbl->setVisible(!lbl->isVisible());
}

}  // namespace binding
}  // namespace pv
