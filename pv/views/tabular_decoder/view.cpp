/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
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

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QVBoxLayout>

#include <libsigrokdecode/libsigrokdecode.h>

#include "view.hpp"

#include "pv/globalsettings.hpp"
#include "pv/session.hpp"
#include "pv/util.hpp"
#include "pv/data/decode/decoder.hpp"

using pv::data::DecodeSignal;
using pv::data::SignalBase;
using pv::data::decode::Decoder;
using pv::util::Timestamp;

using std::make_shared;
using std::shared_ptr;

namespace pv {
namespace views {
namespace tabular_decoder {

QSize QCustomTableView::minimumSizeHint() const
{
	QSize size(QTableView::sizeHint());

	int width = 0;
	for (int i = 0; i < horizontalHeader()->count(); i++)
		if (!horizontalHeader()->isSectionHidden(i))
			width += horizontalHeader()->sectionSize(i);

	size.setWidth(width + (horizontalHeader()->count() * 1));

	return size;
}

QSize QCustomTableView::sizeHint() const
{
	return minimumSizeHint();
}


View::View(Session &session, bool is_main_view, QMainWindow *parent) :
	ViewBase(session, is_main_view, parent),

	// Note: Place defaults in View::reset_view_state(), not here
	parent_(parent),
	decoder_selector_(new QComboBox()),
	save_button_(new QToolButton()),
	save_action_(new QAction(this)),
	table_view_(new QCustomTableView()),
	model_(new AnnotationCollectionModel()),
	signal_(nullptr),
	updating_data_(false)
{
	QVBoxLayout *root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);
	root_layout->addWidget(table_view_);

	// Create toolbar
	QToolBar* toolbar = new QToolBar();
	toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
	parent->addToolBar(toolbar);

	// Populate toolbar
	toolbar->addWidget(new QLabel(tr("Decoder:")));
	toolbar->addWidget(decoder_selector_);
	toolbar->addSeparator();
	toolbar->addWidget(save_button_);

	connect(decoder_selector_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_selected_decoder_changed(int)));

	// Configure widgets
	decoder_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

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

	save_button_->setMenu(save_menu);
	save_button_->setDefaultAction(save_action_);
	save_button_->setPopupMode(QToolButton::MenuButtonPopup);

	// Set up the table view
	table_view_->setModel(model_);
	table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
	table_view_->setSortingEnabled(true);
	table_view_->sortByColumn(0, Qt::AscendingOrder);

	const int font_height = QFontMetrics(QApplication::font()).height();
	table_view_->verticalHeader()->setDefaultSectionSize((font_height * 5) / 4);

	table_view_->horizontalHeader()->setStretchLastSection(true);
	table_view_->horizontalHeader()->setCascadingSectionResizes(true);
	table_view_->horizontalHeader()->setSectionsMovable(true);
	table_view_->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

	table_view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	parent->setSizePolicy(table_view_->sizePolicy());

	connect(table_view_, SIGNAL(clicked(const QModelIndex&)),
		this, SLOT(on_table_item_clicked(const QModelIndex&)));
	connect(table_view_, SIGNAL(doubleClicked(const QModelIndex&)),
		this, SLOT(on_table_item_double_clicked(const QModelIndex&)));
	connect(table_view_->horizontalHeader(), SIGNAL(customContextMenuRequested(const QPoint&)),
		this, SLOT(on_table_header_requested(const QPoint&)));

	reset_view_state();
}

ViewType View::get_type() const
{
	return ViewTypeTabularDecoder;
}

void View::reset_view_state()
{
	ViewBase::reset_view_state();

	decoder_selector_->clear();
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

	// Note: At time of initial creation, decode signals have no decoders so we
	// need to watch for decoder stacking events

	connect(signal.get(), SIGNAL(decoder_stacked(void*)),
		this, SLOT(on_decoder_stacked(void*)));
	connect(signal.get(), SIGNAL(decoder_removed(void*)),
		this, SLOT(on_decoder_removed(void*)));

	// Add the top-level decoder provided by an already-existing signal
	auto stack = signal->decoder_stack();
	if (!stack.empty()) {
		shared_ptr<Decoder>& dec = stack.at(0);
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
	ViewBase::save_settings(settings);
}

void View::restore_settings(QSettings &settings)
{
	// Note: It is assumed that this function is only called once,
	// immediately after restoring a previous session.
	ViewBase::restore_settings(settings);
}

void View::reset_data()
{
	signal_ = nullptr;
	decoder_ = nullptr;
}

void View::update_data()
{
	if (updating_data_) {
		if (!delayed_view_updater_.isActive())
			delayed_view_updater_.start();
		return;
	}

	updating_data_ = true;

	table_view_->setRootIndex(model_->index(1, 0, QModelIndex()));
	model_->set_signal_and_segment(signal_, current_segment_);

	updating_data_ = false;
}

void View::save_data() const
{
	assert(decoder_);
	assert(signal_);

	if (!signal_)
		return;

/*	GlobalSettings settings;
	const QString dir = settings.value("MainWindow/SaveDirectory").toString();

	const QString file_name = QFileDialog::getSaveFileName(
		parent_, tr("Save Binary Data"), dir, tr("Binary Data Files (*.bin);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		pair<size_t, size_t> selection = hex_view_->get_selection();

		vector<uint8_t> data;
		data.resize(selection.second - selection.first + 1);

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
	} */
}

void View::on_selected_decoder_changed(int index)
{
	if (signal_) {
		disconnect(signal_, SIGNAL(color_changed(QColor)));
		disconnect(signal_, SIGNAL(new_annotations()));
		disconnect(signal_, SIGNAL(decode_reset()));
	}

	reset_data();

	decoder_ = (Decoder*)decoder_selector_->itemData(index).value<void*>();

	// Find the signal that contains the selected decoder
	for (const shared_ptr<DecodeSignal>& ds : decode_signals_)
		for (const shared_ptr<Decoder>& dec : ds->decoder_stack())
			if (decoder_ == dec.get())
				signal_ = ds.get();

	if (signal_) {
		connect(signal_, SIGNAL(color_changed(QColor)), this, SLOT(on_signal_color_changed(QColor)));
		connect(signal_, SIGNAL(new_annotations()), this, SLOT(on_new_annotations()));
		connect(signal_, SIGNAL(decode_reset()), this, SLOT(on_decoder_reset()));
	}

	update_data();
}

void View::on_signal_name_changed(const QString &name)
{
	(void)name;

	SignalBase* sb = qobject_cast<SignalBase*>(QObject::sender());
	assert(sb);

	DecodeSignal* signal = dynamic_cast<DecodeSignal*>(sb);
	assert(signal);

	// Update the top-level decoder provided by this signal
	auto stack = signal->decoder_stack();
	if (!stack.empty()) {
		shared_ptr<Decoder>& dec = stack.at(0);
		int index = decoder_selector_->findData(QVariant::fromValue((void*)dec.get()));

		if (index != -1)
			decoder_selector_->setItemText(index, signal->name());
	}
}

void View::on_signal_color_changed(const QColor &color)
{
	(void)color;

	table_view_->update();
}

void View::on_new_annotations()
{
	if (!delayed_view_updater_.isActive())
		delayed_view_updater_.start();
}

void View::on_decoder_reset()
{
	// Invalidate the model's data connection immediately - otherwise we
	// will use a stale pointer in model_->index() when called from the table view
	model_->set_signal_and_segment(signal_, current_segment_);
}

void View::on_decoder_stacked(void* decoder)
{
	Decoder* d = static_cast<Decoder*>(decoder);

	// Find the signal that contains the selected decoder
	DecodeSignal* signal = nullptr;

	for (const shared_ptr<DecodeSignal>& ds : decode_signals_)
		for (const shared_ptr<Decoder>& dec : ds->decoder_stack())
			if (d == dec.get())
				signal = ds.get();

	assert(signal);

	const shared_ptr<Decoder>& dec = signal->decoder_stack().at(0);
	int index = decoder_selector_->findData(QVariant::fromValue((void*)dec.get()));

	if (index == -1) {
		// Add the decoder to the list
		decoder_selector_->addItem(signal->name(), QVariant::fromValue((void*)d));
	}
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
	(void)action;

	save_data();
}

void View::on_table_item_clicked(const QModelIndex& index)
{
	(void)index;

	// Force repaint, otherwise the new selection isn't shown for some reason
	table_view_->viewport()->update();
}

void View::on_table_item_double_clicked(const QModelIndex& index)
{
	const Annotation* ann = static_cast<const Annotation*>(index.internalPointer());

	shared_ptr<views::ViewBase> main_view = session_.main_view();

	main_view->focus_on_range(ann->start_sample(), ann->end_sample());
}

void View::on_table_header_requested(const QPoint& pos)
{
	QMenu* menu = new QMenu(this);

	for (int i = 0; i < table_view_->horizontalHeader()->count(); i++) {
		int column = table_view_->horizontalHeader()->logicalIndex(i);

		const QString title = model_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
		QAction* action = new QAction(title, this);

		action->setCheckable(true);
		action->setChecked(!table_view_->horizontalHeader()->isSectionHidden(column));
		action->setData(column);

		connect(action, SIGNAL(toggled(bool)), this, SLOT(on_table_header_toggled(bool)));

		menu->addAction(action);
	}

	menu->popup(table_view_->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void View::on_table_header_toggled(bool checked)
{
	QAction* action = qobject_cast<QAction*>(QObject::sender());
	assert(action);

	const int column = action->data().toInt();

	table_view_->horizontalHeader()->setSectionHidden(column, !checked);
}

void View::perform_delayed_view_update()
{
	update_data();
}


} // namespace tabular_decoder
} // namespace views
} // namespace pv
