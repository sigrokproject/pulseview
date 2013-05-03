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

#ifdef ENABLE_SIGROKDECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <QTextDocument>

#include "about.h"
#include <ui_about.h>

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <glib.h>
#include <libsigrok/libsigrok.h>


namespace pv {
namespace dialogs {

About::About(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::About)
{
	struct sr_dev_driver **drivers;
	struct sr_input_format **inputs;
	struct sr_output_format **outputs;

#ifdef ENABLE_SIGROKDECODE
	struct srd_decoder *dec;
#endif

	QString s;

	ui->setupUi(this);

	/* Setup the version field */
	ui->versionInfo->setText(tr("%1 %2<br />%3<br /><a href=\"%4\">%4</a>")
				 .arg(QApplication::applicationName())
				 .arg(QApplication::applicationVersion())
				 .arg(tr("GNU GPL, version 3 or later"))
				 .arg(QApplication::organizationDomain()));
	ui->versionInfo->setOpenExternalLinks(true);

	s.append("<table>");

	/* Set up the supported field */
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported hardware drivers:") +
		"</b></td></tr>");
	drivers = sr_driver_list();
	for (int i = 0; drivers[i]; ++i) {
		s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
			 .arg(QString(drivers[i]->name))
			 .arg(QString(drivers[i]->longname)));
	}

	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported input formats:") +
		"</b></td></tr>");
	inputs = sr_input_list();
	for (int i = 0; inputs[i]; ++i) {
		s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
			 .arg(QString(inputs[i]->id))
			 .arg(QString(inputs[i]->description)));
	}

	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported output formats:") +
		"</b></td></tr>");
	outputs = sr_output_list();
	for (int i = 0; outputs[i]; ++i) {
		s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
			.arg(QString(outputs[i]->id))
			.arg(QString(outputs[i]->description)));
	}

#ifdef ENABLE_SIGROKDECODE
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported protocol decoders:") +
		"</b></td></tr>");
	for (const GSList *l = srd_decoder_list(); l; l = l->next) {
		dec = (struct srd_decoder *)l->data;
		s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
			 .arg(QString(dec->id))
			 .arg(QString(dec->longname)));
	}
#endif

	s.append("</table>");

	supportedDoc.reset(new QTextDocument(this));
	supportedDoc->setHtml(s);
	ui->supportList->setDocument(supportedDoc.get());
}

About::~About()
{
	delete ui;
}

} // namespace dialogs
} // namespace pv
