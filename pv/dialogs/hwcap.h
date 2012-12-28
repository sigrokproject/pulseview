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

#ifndef PULSEVIEW_PV_HWCAP_H
#define PULSEVIEW_PV_HWCAP_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include <pv/prop/binding/hwcap.h>

namespace pv {
namespace dialogs {

class HwCap : public QDialog
{
public:
	HwCap(QWidget *parent, struct sr_dev_inst *sdi);

protected:
	void accept();

private:
	QVBoxLayout _layout;
	QDialogButtonBox _button_box;

	pv::prop::binding::HwCap _hw_cap_binding;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_HWCAP_H
