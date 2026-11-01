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

#include <QCheckBox>
#include <QDebug>
#include <QSignalBlocker>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "bool.hpp"
#include "configerror.hpp"

namespace pv {
namespace prop {

Bool::Bool(QString name, QString desc, Getter getter, Setter setter,
	bool allow_unknown) :
	Property(name, desc, getter, setter),
	check_box_(nullptr),
	allow_unknown_(allow_unknown)
{
}

QWidget* Bool::get_widget(QWidget *parent, bool auto_commit)
{
	if (check_box_)
		return check_box_;

	if (!getter_)
		return nullptr;

	try {
		Glib::VariantBase variant = getter_();
		if (!variant.gobj())
			return nullptr;
	} catch (const sigrok::Error &e) {
		if (!allow_unknown_ || e.result != SR_ERR_NA) {
			qWarning() << tr("Querying config key %1 resulted in %2").arg(name_, e.what());
			return nullptr;
		}
	}

	check_box_ = new QCheckBox(name(), parent);
	check_box_->setToolTip(desc());

	update_widget();

	if (auto_commit)
		connect(check_box_, SIGNAL(stateChanged(int)),
			this, SLOT(on_state_changed(int)));

	return check_box_;
}

bool Bool::labeled_widget() const
{
	return true;
}

void Bool::update_widget()
{
	if (!check_box_)
		return;
	const QSignalBlocker blocker(check_box_);

	Glib::VariantBase variant;

	try {
		variant = getter_();
	} catch (const sigrok::Error &e) {
		if (allow_unknown_ && e.result == SR_ERR_NA) {
			check_box_->setEnabled(true);
			check_box_->setTristate(true);
			check_box_->setCheckState(Qt::PartiallyChecked);
			check_box_->setToolTip(tr("Output state is unknown. Select enabled or disabled to send an explicit command. Frequency and duty are requested settings until applied."));
			return;
		}
		qWarning() << tr("Querying config key %1 resulted in %2").arg(name_, e.what());
		check_box_->setEnabled(false);
		return;
	}

	assert(variant.gobj());
	check_box_->setEnabled(true);
	bool value = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(
		variant).get();

	check_box_->setTristate(false);
	check_box_->setCheckState(value ? Qt::Checked : Qt::Unchecked);
	check_box_->setToolTip(desc());
}

void Bool::commit()
{
	assert(setter_);

	if (!check_box_ || !check_box_->isEnabled())
		return;

	if (check_box_->checkState() == Qt::PartiallyChecked)
		return;
	try {
		setter_(Glib::Variant<bool>::create(check_box_->checkState() == Qt::Checked));
		update_widget();
	} catch (const sigrok::Error &e) {
		update_widget();
		if (e.result == SR_ERR_DEV_CLOSED) check_box_->setEnabled(false);
		show_config_error(check_box_, e);
	}
}

void Bool::on_state_changed(int)
{
	commit();
}

}  // namespace prop
}  // namespace pv
