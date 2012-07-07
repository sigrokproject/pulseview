/*
 * This file is part of the sigrok project.
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

extern "C" {
#include <sigrokdecode.h>
}

#include <QFileDialog>

#include "about.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "samplingbar.h"
#include "sigview.h"

extern "C" {
/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
}

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	_ui(new Ui::MainWindow)
{
	_ui->setupUi(this);

	_sampling_bar = new SamplingBar(this);
	connect(_sampling_bar, SIGNAL(run_stop()), this,
		SLOT(run_stop()));
	addToolBar(_sampling_bar);

	_view = new SigView(_session, this);
	_ui->verticalLayout->addWidget(_view);
}

MainWindow::~MainWindow()
{
	delete _ui;
}

void MainWindow::on_actionOpen_triggered()
{
	QString file_name = QFileDialog::getOpenFileName(
		this, tr("Open File"), "",
		tr("Sigrok Sessions (*.sr)"));
	_session.load_file(file_name.toStdString());
}

void MainWindow::on_actionAbout_triggered()
{
	About dlg(this);
	dlg.exec();
}

void MainWindow::run_stop()
{
	_session.start_capture(
		_sampling_bar->get_selected_device(),
		_sampling_bar->get_record_length(),
		_sampling_bar->get_sample_rate());
}
