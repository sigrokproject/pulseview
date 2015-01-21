/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONS_HPP
#define PULSEVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONS_HPP

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>

#include <pv/binding/inputoutput.hpp>

namespace pv {
namespace dialogs {

/**
 * Presents the selection of inpout/output options to the user.
 */
class InputOutputOptions : public QDialog
{
	Q_OBJECT

public:
	/**
	 * Constructor.
	 * @param title the title of the dialog.
	 * @param options the map of options to use as a template.
	 * @param parent the parent widget of the dialog.
	 */ 
	InputOutputOptions(const QString &title,
		const std::map<std::string, std::shared_ptr<sigrok::Option>>
			&options,
		QWidget *parent);

	/**
	 * Gets the map of selected options.
	 * @return the options.
	 */
	const std::map<std::string, Glib::VariantBase>& options() const;

protected:
	void accept();

private:
	QVBoxLayout layout_;
	QDialogButtonBox button_box_;
	pv::binding::InputOutput binding_;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_INPUTOUTPUTOPTIONS_HPP
