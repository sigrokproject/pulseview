/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2019 Soeren Apel <soeren@apelpie.net>
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

#include <climits>

#include <QByteArray>
#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QVBoxLayout>

#include <libsigrokdecode/libsigrokdecode.h>

#include "view.hpp"
#include "QHexView.hpp"

#include "pv/globalsettings.hpp"
#include "pv/session.hpp"
#include "pv/util.hpp"
#include "pv/data/decode/decoder.hpp"

using pv::data::DecodeSignal;
using pv::data::SignalBase;
using pv::data::decode::Decoder;
using pv::util::Timestamp;

using std::shared_ptr;

namespace pv {
namespace views {
namespace decoder_output {

const char* SaveTypeNames[SaveTypeCount] = {
	"Binary",
	"Hex Dump, plain",
	"Hex Dump, with offset",
	"Hex Dump, canonical"
};


View::View(Session &session, bool is_main_view, QMainWindow *parent) :
	ViewBase(session, is_main_view, parent),

	// Note: Place defaults in View::reset_view_state(), not here
	parent_(parent),
	decoder_selector_(new QComboBox()),
	format_selector_(new QComboBox()),
	class_selector_(new QComboBox()),
	stacked_widget_(new QStackedWidget()),
	hex_view_(new QHexView()),
	save_button_(new QToolButton()),
	save_action_(new QAction(this)),
	signal_(nullptr)
{
	QVBoxLayout *root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);

	// Create toolbar
	QToolBar* toolbar = new QToolBar();
	toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
	parent->addToolBar(toolbar);

	// Populate toolbar
	toolbar->addWidget(new QLabel(tr("Decoder:")));
	toolbar->addWidget(decoder_selector_);
	toolbar->addWidget(class_selector_);
	toolbar->addSeparator();
	toolbar->addWidget(new QLabel(tr("Show data as")));
	toolbar->addWidget(format_selector_);
	toolbar->addSeparator();
	toolbar->addWidget(save_button_);

	// Add format types
	format_selector_->addItem(tr("Hexdump"), qVariantFromValue(QString("text/hexdump")));

	// Add widget stack
	root_layout->addWidget(stacked_widget_);
	stacked_widget_->addWidget(hex_view_);
	stacked_widget_->setCurrentIndex(0);

	connect(decoder_selector_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_selected_decoder_changed(int)));
	connect(class_selector_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_selected_class_changed(int)));

	// Configure widgets
	decoder_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	class_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	// Configure actions
	save_action_->setText(tr("&Save..."));
	save_action_->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	save_action_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	connect(save_action_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSave_triggered()));

	QMenu *save_menu = new QMenu();
	connect(save_menu, SIGNAL(triggered(QAction*)),
		this, SLOT(on_actionSave_triggered(QAction*)));

	for (int i = 0; i < SaveTypeCount; i++) {
		QAction *const action =	save_menu->addAction(tr(SaveTypeNames[i]));
		action->setData(qVariantFromValue(i));
	}

	save_button_->setMenu(save_menu);
	save_button_->setDefaultAction(save_action_);
	save_button_->setPopupMode(QToolButton::MenuButtonPopup);

	parent->setSizePolicy(hex_view_->sizePolicy()); // TODO Must be updated when selected widget changes

	reset_view_state();
}

ViewType View::get_type() const
{
	return ViewTypeDecoderOutput;
}

void View::reset_view_state()
{
	ViewBase::reset_view_state();

	decoder_selector_->clear();
	class_selector_->clear();
	format_selector_->setCurrentIndex(0);
	save_button_->setEnabled(false);

	hex_view_->clear();
}

void View::clear_decode_signals()
{
	ViewBase::clear_decode_signals();

	reset_data();
	reset_view_state();
}

void View::add_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	ViewBase::add_decode_signal(signal);

	connect(signal.get(), SIGNAL(name_changed(const QString&)),
		this, SLOT(on_signal_name_changed(const QString&)));
	connect(signal.get(), SIGNAL(decoder_stacked(void*)),
		this, SLOT(on_decoder_stacked(void*)));
	connect(signal.get(), SIGNAL(decoder_removed(void*)),
		this, SLOT(on_decoder_removed(void*)));

	// Add all decoders provided by this signal
	auto stack = signal->decoder_stack();
	if (stack.size() > 1) {
		for (const shared_ptr<Decoder>& dec : stack)
			// Only add the decoder if it has binary output
			if (dec->get_binary_class_count() > 0) {
				QString title = QString("%1 (%2)").arg(signal->name(), dec->name());
				decoder_selector_->addItem(title, QVariant::fromValue((void*)dec.get()));
			}
	} else
		if (!stack.empty()) {
			shared_ptr<Decoder>& dec = stack.at(0);
			if (dec->get_binary_class_count() > 0)
				decoder_selector_->addItem(signal->name(), QVariant::fromValue((void*)dec.get()));
		}
}

void View::remove_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	// Remove all decoders provided by this signal
	for (const shared_ptr<Decoder>& dec : signal->decoder_stack()) {
		int index = decoder_selector_->findData(QVariant::fromValue((void*)dec.get()));

		if (index != -1)
			decoder_selector_->removeItem(index);
	}

	ViewBase::remove_decode_signal(signal);

	if (signal.get() == signal_) {
		reset_data();
		update_data();
		reset_view_state();
	}
}

void View::save_settings(QSettings &settings) const
{
	(void)settings;
}

void View::restore_settings(QSettings &settings)
{
	// Note: It is assumed that this function is only called once,
	// immediately after restoring a previous session.
	(void)settings;
}

void View::reset_data()
{
	signal_ = nullptr;
	decoder_ = nullptr;
	bin_class_id_ = 0;
	binary_data_exists_ = false;

	hex_view_->clear();
}

void View::update_data()
{
	if (!signal_)
		return;

	const DecodeBinaryClass* bin_class =
		signal_->get_binary_data_class(current_segment_, decoder_, bin_class_id_);

	hex_view_->set_data(bin_class);

	if (!binary_data_exists_)
		return;

	if (!save_button_->isEnabled())
		save_button_->setEnabled(true);
}

void View::save_data() const
{
	assert(decoder_);
	assert(signal_);

	if (!signal_)
		return;

	GlobalSettings settings;
	const QString dir = settings.value("MainWindow/SaveDirectory").toString();

	const QString file_name = QFileDialog::getSaveFileName(
		parent_, tr("Save Binary Data"), dir, tr("Binary Data Files (*.bin);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		pair<size_t, size_t> selection = hex_view_->get_selection();

		vector<uint8_t> data;
		signal_->get_merged_binary_data_chunks_by_offset(current_segment_, decoder_,
			bin_class_id_, selection.first, selection.second, &data);

		int64_t bytes_written = file.write((const char*)data.data(), data.size());

		if ((bytes_written == -1) || ((uint64_t)bytes_written != data.size())) {
			QMessageBox msg(parent_);
			msg.setText(tr("Error") + "\n\n" + tr("File %1 could not be written to.").arg(file_name));
			msg.setStandardButtons(QMessageBox::Ok);
			msg.setIcon(QMessageBox::Warning);
			msg.exec();
			return;
		}
	}
}

void View::save_data_as_hex_dump(bool with_offset, bool with_ascii) const
{
	assert(decoder_);
	assert(signal_);

	if (!signal_)
		return;

	GlobalSettings settings;
	const QString dir = settings.value("MainWindow/SaveDirectory").toString();

	const QString file_name = QFileDialog::getSaveFileName(
		parent_, tr("Save Binary Data"), dir, tr("Hex Dumps (*.txt);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		pair<size_t, size_t> selection = hex_view_->get_selection();

		vector<uint8_t> data;
		signal_->get_merged_binary_data_chunks_by_offset(current_segment_, decoder_,
			bin_class_id_, selection.first, selection.second, &data);

		QTextStream out_stream(&file);

		uint64_t offset = selection.first;
		uint64_t n = hex_view_->get_bytes_per_line();
		QString s;

		while (offset < selection.second) {
			size_t end = std::min((uint64_t)(selection.second), offset + n);
			offset = hex_view_->create_hex_line(offset, end, &s, with_offset, with_ascii);
			out_stream << s << endl;
		}

		out_stream << endl;

		if (out_stream.status() != QTextStream::Ok) {
			QMessageBox msg(parent_);
			msg.setText(tr("Error") + "\n\n" + tr("File %1 could not be written to.").arg(file_name));
			msg.setStandardButtons(QMessageBox::Ok);
			msg.setIcon(QMessageBox::Warning);
			msg.exec();
			return;
		}
	}
}

void View::on_selected_decoder_changed(int index)
{
	if (signal_)
		disconnect(signal_, SIGNAL(new_binary_data(unsigned int, void*, unsigned int)));

	reset_data();

	decoder_ = (Decoder*)decoder_selector_->itemData(index).value<void*>();

	// Find the signal that contains the selected decoder
	for (const shared_ptr<DecodeSignal>& ds : decode_signals_)
		for (const shared_ptr<Decoder>& dec : ds->decoder_stack())
			if (decoder_ == dec.get())
				signal_ = ds.get();

	class_selector_->clear();

	if (signal_) {
		// Populate binary class selector
		uint32_t bin_classes = decoder_->get_binary_class_count();
		for (uint32_t i = 0; i < bin_classes; i++) {
			const data::decode::DecodeBinaryClassInfo* class_info = decoder_->get_binary_class(i);
			class_selector_->addItem(class_info->description, QVariant::fromValue(i));
		}

		connect(signal_, SIGNAL(new_binary_data(unsigned int, void*, unsigned int)),
			this, SLOT(on_new_binary_data(unsigned int, void*, unsigned int)));
	}

	update_data();
}

void View::on_selected_class_changed(int index)
{
	bin_class_id_ = class_selector_->itemData(index).value<uint32_t>();

	binary_data_exists_ =
		signal_->get_binary_data_chunk_count(current_segment_, decoder_, bin_class_id_);

	update_data();
}

void View::on_signal_name_changed(const QString &name)
{
	(void)name;

	SignalBase* sb = qobject_cast<SignalBase*>(QObject::sender());
	assert(sb);

	DecodeSignal* signal = dynamic_cast<DecodeSignal*>(sb);
	assert(signal);

	// Update all decoder entries provided by this signal
	auto stack = signal->decoder_stack();
	if (stack.size() > 1) {
		for (const shared_ptr<Decoder>& dec : stack) {
			QString title = QString("%1 (%2)").arg(signal->name(), dec->name());
			int index = decoder_selector_->findData(QVariant::fromValue((void*)dec.get()));

			if (index != -1)
				decoder_selector_->setItemText(index, title);
		}
	} else
		if (!stack.empty()) {
			shared_ptr<Decoder>& dec = stack.at(0);
			int index = decoder_selector_->findData(QVariant::fromValue((void*)dec.get()));

			if (index != -1)
				decoder_selector_->setItemText(index, signal->name());
		}
}

void View::on_new_binary_data(unsigned int segment_id, void* decoder, unsigned int bin_class_id)
{
	if ((segment_id == current_segment_) && (decoder == decoder_) && (bin_class_id == bin_class_id_))
		if (!delayed_view_updater_.isActive())
			delayed_view_updater_.start();
}

void View::on_decoder_stacked(void* decoder)
{
	// TODO This doesn't change existing entries for the same signal - but it should as the naming scheme may change

	Decoder* d = static_cast<Decoder*>(decoder);

	// Only add the decoder if it has binary output
	if (d->get_binary_class_count() == 0)
		return;

	// Find the signal that contains the selected decoder
	DecodeSignal* signal = nullptr;

	for (const shared_ptr<DecodeSignal>& ds : decode_signals_)
		for (const shared_ptr<Decoder>& dec : ds->decoder_stack())
			if (d == dec.get())
				signal = ds.get();

	assert(signal);

	// Add the decoder to the list
	QString title = QString("%1 (%2)").arg(signal->name(), d->name());
	decoder_selector_->addItem(title, QVariant::fromValue((void*)d));
}

void View::on_decoder_removed(void* decoder)
{
	Decoder* d = static_cast<Decoder*>(decoder);

	// Remove the decoder from the list
	int index = decoder_selector_->findData(QVariant::fromValue((void*)d));

	if (index != -1)
		decoder_selector_->removeItem(index);
}

void View::on_actionSave_triggered(QAction* action)
{
	int save_type = SaveTypeBinary;
	if (action)
		save_type = action->data().toInt();

	switch (save_type)
	{
	case SaveTypeBinary: save_data(); break;
	case SaveTypeHexDumpPlain: save_data_as_hex_dump(false, false); break;
	case SaveTypeHexDumpWithOffset: save_data_as_hex_dump(true, false); break;
	case SaveTypeHexDumpComplete: save_data_as_hex_dump(true, true); break;
	}
}

void View::perform_delayed_view_update()
{
	if (!binary_data_exists_)
		if (signal_->get_binary_data_chunk_count(current_segment_, decoder_, bin_class_id_))
			binary_data_exists_ = true;

	update_data();
}


} // namespace decoder_output
} // namespace views
} // namespace pv
