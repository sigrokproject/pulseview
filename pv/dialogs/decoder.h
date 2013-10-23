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

#ifndef PULSEVIEW_PV_DECODER_H
#define PULSEVIEW_PV_DECODER_H

#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <pv/prop/binding/decoderoptions.h>

struct srd_decoder;

namespace pv {

namespace view {
class Signal;
}

namespace dialogs {

class Decoder : public QDialog
{
public:
	Decoder(QWidget *parent, const srd_decoder *decoder,
		const std::vector< boost::shared_ptr<view::Signal> > &sigs,
		GHashTable *options);

	void accept();

	std::map<const srd_probe*, boost::shared_ptr<view::Signal> >
		get_probes();

private:
	QComboBox* create_probe_selector(
		QWidget *parent, const char *name);

private:
	const std::vector< boost::shared_ptr<view::Signal> > &_sigs;

	std::map<const srd_probe*, QComboBox*> _probe_selector_map;

	pv::prop::binding::DecoderOptions _binding;

	QVBoxLayout _layout;

	QWidget _form;
	QFormLayout _form_layout;

	QLabel _heading;
	QDialogButtonBox _button_box;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_DECODER_H
