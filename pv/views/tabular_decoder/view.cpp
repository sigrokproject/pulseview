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
using std::max;
using std::shared_ptr;

namespace pv {
namespace views {
namespace tabular_decoder {

const char* SaveTypeNames[SaveTypeCount] = {
	"CSV, commas escaped",
	"CSV, fields quoted"
};

const char* ViewModeNames[ViewModeCount] = {
	"Show all",
	"Show all and focus on newest",
	"Show visible in main view"
};


CustomFilterProxyModel::CustomFilterProxyModel(QObject* parent) :
	QSortFilterProxyModel(parent),
	range_filtering_enabled_(false)
{
}

bool CustomFilterProxyModel::filterAcceptsRow(int sourceRow,
	const QModelIndex &sourceParent) const
{
	(void)sourceParent;
	assert(sourceModel() != nullptr);

	bool result = true;

	if (range_filtering_enabled_) {
		const QModelIndex ann_start_sample_idx = sourceModel()->index(sourceRow, 0);
		const uint64_t ann_start_sample =
			sourceModel()->data(ann_start_sample_idx, Qt::DisplayRole).toULongLong();

		const QModelIndex ann_end_sample_idx = sourceModel()->index(sourceRow, 6);
		const uint64_t ann_end_sample =
			sourceModel()->data(ann_end_sample_idx, Qt::DisplayRole).toULongLong();

		// We consider all annotations as visible that either
		// a) begin to the left of the range and end within the range or
		// b) begin and end within the range or
		// c) begin within the range and end to the right of the range
		// ...which is equivalent to the negation of "begins and ends outside the range"

		const bool left_of_range = (ann_end_sample < range_start_sample_);
		const bool right_of_range = (ann_start_sample > range_end_sample_);
		const bool entirely_outside_of_range = left_of_range || right_of_range;

		result = !entirely_outside_of_range;
	}

	return result;
}

void CustomFilterProxyModel::set_sample_range(uint64_t start_sample,
	uint64_t end_sample)
{
	range_start_sample_ = start_sample;
	range_end_sample_ = end_sample;

	invalidateFilter();
}

void CustomFilterProxyModel::enable_range_filtering(bool value)
{
	range_filtering_enabled_ = value;

	invalidateFilter();
}


QSize CustomTableView::minimumSizeHint() const
{
	QSize size(QTableView::sizeHint());

	int width = 0;
	for (int i = 0; i < horizontalHeader()->count(); i++)
		if (!horizontalHeader()->isSectionHidden(i))
			width += horizontalHeader()->sectionSize(i);

	size.setWidth(width + (horizontalHeader()->count() * 1));

	return size;
}

QSize CustomTableView::sizeHint() const
{
	return minimumSizeHint();
}

void CustomTableView::keyPressEvent(QKeyEvent *event)
{
	if ((event->key() == Qt::Key_Return) || (event->key() == Qt::Key_Enter))
		activatedByKey(currentIndex());
	else
		QTableView::keyPressEvent(event);
}


View::View(Session &session, bool is_main_view, QMainWindow *parent) :
	ViewBase(session, is_main_view, parent),

	// Note: Place defaults in View::reset_view_state(), not here
	parent_(parent),
	decoder_selector_(new QComboBox()),
	hide_hidden_cb_(new QCheckBox()),
	view_mode_selector_(new QComboBox()),
	save_button_(new QToolButton()),
	save_action_(new QAction(this)),
	table_view_(new CustomTableView()),
	model_(new AnnotationCollectionModel(this)),
	filter_proxy_model_(new CustomFilterProxyModel(this)),
	signal_(nullptr)
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
	toolbar->addSeparator();
	toolbar->addWidget(view_mode_selector_);
	toolbar->addSeparator();
	toolbar->addWidget(hide_hidden_cb_);

	connect(decoder_selector_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_selected_decoder_changed(int)));
	connect(view_mode_selector_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_view_mode_changed(int)));
	connect(hide_hidden_cb_, SIGNAL(toggled(bool)),
		this, SLOT(on_hide_hidden_changed(bool)));

	// Configure widgets
	decoder_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	for (int i = 0; i < ViewModeCount; i++)
		view_mode_selector_->addItem(ViewModeNames[i], QVariant::fromValue(i));

	hide_hidden_cb_->setText(tr("Hide Hidden Rows/Classes"));
	hide_hidden_cb_->setChecked(true);

	// Configure actions
	save_action_->setText(tr("&Save..."));
	save_action_->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	save_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
#else
	save_action_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
#endif
	connect(save_action_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSave_triggered()));

	QMenu *save_menu = new QMenu();
	connect(save_menu, SIGNAL(triggered(QAction*)),
		this, SLOT(on_actionSave_triggered(QAction*)));

	for (int i = 0; i < SaveTypeCount; i++) {
		QAction *const action =	save_menu->addAction(tr(SaveTypeNames[i]));
		action->setData(QVariant::fromValue(i));
	}

	save_button_->setMenu(save_menu);
	save_button_->setDefaultAction(save_action_);
	save_button_->setPopupMode(QToolButton::MenuButtonPopup);

	// Set up the models and the table view
	filter_proxy_model_->setSourceModel(model_);
	table_view_->setModel(filter_proxy_model_);

	table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_view_->setSelectionMode(QAbstractItemView::ContiguousSelection);
	table_view_->setSortingEnabled(true);
	table_view_->sortByColumn(0, Qt::AscendingOrder);

	for (uint8_t i = model_->first_hidden_column(); i < model_->columnCount(); i++)
		table_view_->setColumnHidden(i, true);

	const int font_height = QFontMetrics(QApplication::font()).height();
	table_view_->verticalHeader()->setDefaultSectionSize((font_height * 5) / 4);
	table_view_->verticalHeader()->setVisible(false);

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
	connect(table_view_, SIGNAL(activatedByKey(const QModelIndex&)),
		this, SLOT(on_table_item_double_clicked(const QModelIndex&)));
	connect(table_view_->horizontalHeader(), SIGNAL(customContextMenuRequested(const QPoint&)),
		this, SLOT(on_table_header_requested(const QPoint&)));

	// Set up metadata event handler
	session_.metadata_obj_manager()->add_observer(this);

	reset_view_state();
}

View::~View()
{
	session_.metadata_obj_manager()->remove_observer(this);
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

	settings.setValue("view_mode", view_mode_selector_->currentIndex());
	settings.setValue("hide_hidden", hide_hidden_cb_->isChecked());
}

void View::restore_settings(QSettings &settings)
{
	ViewBase::restore_settings(settings);

	if (settings.contains("view_mode"))
		view_mode_selector_->setCurrentIndex(settings.value("view_mode").toInt());

	if (settings.contains("hide_hidden"))
		hide_hidden_cb_->setChecked(settings.value("hide_hidden").toBool());
}

void View::reset_data()
{
	signal_ = nullptr;
	decoder_ = nullptr;
}

void View::update_data()
{
	model_->set_signal_and_segment(signal_, current_segment_);
}

void View::save_data_as_csv(unsigned int save_type) const
{
	// Note: We try to follow RFC 4180 (https://tools.ietf.org/html/rfc4180)

	assert(decoder_);
	assert(signal_);

	if (!signal_)
		return;

	const bool save_all = !table_view_->selectionModel()->hasSelection();

	GlobalSettings settings;
	const QString dir = settings.value("MainWindow/SaveDirectory").toString();

	const QString file_name = QFileDialog::getSaveFileName(
		parent_, tr("Save Annotations as CSV"), dir, tr("CSV Files (*.csv);;Text Files (*.txt);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QTextStream out_stream(&file);

		if (save_all)
			table_view_->selectAll();

		// Write out header columns in visual order, not logical order
		for (int i = 0; i < table_view_->horizontalHeader()->count(); i++) {
			int column = table_view_->horizontalHeader()->logicalIndex(i);

			if (table_view_->horizontalHeader()->isSectionHidden(column))
				continue;

			const QString title = filter_proxy_model_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();

			if (save_type == SaveTypeCSVEscaped)
				out_stream << title;
			else
				out_stream << '"' << title << '"';

			if (i < (table_view_->horizontalHeader()->count() - 1))
				out_stream << ",";
		}
		out_stream << '\r' << '\n';


		QModelIndexList selected_rows = table_view_->selectionModel()->selectedRows();

		for (int i = 0; i < selected_rows.size(); i++) {
			const int row = selected_rows.at(i).row();

			// Write out columns in visual order, not logical order
			for (int c = 0; c < table_view_->horizontalHeader()->count(); c++) {
				const int column = table_view_->horizontalHeader()->logicalIndex(c);

				if (table_view_->horizontalHeader()->isSectionHidden(column))
					continue;

				const QModelIndex idx = filter_proxy_model_->index(row, column);
				QString s = filter_proxy_model_->data(idx, Qt::DisplayRole).toString();

				if (save_type == SaveTypeCSVEscaped)
					out_stream << s.replace(",", "\\,");
				else
					out_stream << '"' << s.replace("\"", "\"\"") << '"';

				if (c < (table_view_->horizontalHeader()->count() - 1))
					out_stream << ",";
			}

			out_stream << '\r' << '\n';
		}

		if (out_stream.status() == QTextStream::Ok) {
			if (save_all)
				table_view_->clearSelection();

			return;
		}
	}

	QMessageBox msg(parent_);
	msg.setText(tr("Error") + "\n\n" + tr("File %1 could not be written to.").arg(file_name));
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
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

	// Force repaint, otherwise the new selection isn't shown for some reason
	table_view_->viewport()->update();
}

void View::on_hide_hidden_changed(bool checked)
{
	model_->set_hide_hidden(checked);

	// Force repaint, otherwise the new selection isn't shown for some reason
	table_view_->viewport()->update();
}

void View::on_view_mode_changed(int index)
{
	if (index == ViewModeAll)
		filter_proxy_model_->enable_range_filtering(false);

	if (index == ViewModeVisible) {
		MetadataObject *md_obj =
			session_.metadata_obj_manager()->find_object_by_type(MetadataObjMainViewRange);
		assert(md_obj);

		int64_t start_sample = md_obj->value(MetadataValueStartSample).toLongLong();
		int64_t end_sample = md_obj->value(MetadataValueEndSample).toLongLong();

		filter_proxy_model_->enable_range_filtering(true);
		filter_proxy_model_->set_sample_range(max((int64_t)0, start_sample),
			max((int64_t)0, end_sample));
	}

	if (index == ViewModeLatest) {
		filter_proxy_model_->enable_range_filtering(false);

		table_view_->scrollTo(
			filter_proxy_model_->mapFromSource(model_->index(model_->rowCount() - 1, 0)),
			QAbstractItemView::PositionAtBottom);
	}
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

	// Force immediate repaint, otherwise it's updated after the header popup is closed
	table_view_->viewport()->update();
}

void View::on_new_annotations()
{
	if (view_mode_selector_->currentIndex() == ViewModeLatest) {
		update_data();
		table_view_->scrollTo(
			filter_proxy_model_->index(filter_proxy_model_->rowCount() - 1, 0),
			QAbstractItemView::PositionAtBottom);
	} else {
		if (!delayed_view_updater_.isActive())
			delayed_view_updater_.start();
	}
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
	int save_type = SaveTypeCSVQuoted;

	if (action)
		save_type = action->data().toInt();

	save_data_as_csv(save_type);
}

void View::on_table_item_clicked(const QModelIndex& index)
{
	(void)index;

	// Force repaint, otherwise the new selection isn't shown for some reason
	table_view_->viewport()->update();
}

void View::on_table_item_double_clicked(const QModelIndex& index)
{
	const QModelIndex src_idx = filter_proxy_model_->mapToSource(index);

	const Annotation* ann = static_cast<const Annotation*>(src_idx.internalPointer());
	assert(ann);

	shared_ptr<views::ViewBase> main_view = session_.main_view();

	main_view->focus_on_range(ann->start_sample(), ann->end_sample());
}

void View::on_table_header_requested(const QPoint& pos)
{
	QMenu* menu = new QMenu(this);

	for (int i = 0; i < table_view_->horizontalHeader()->count(); i++) {
		int column = table_view_->horizontalHeader()->logicalIndex(i);

		const QString title =
			filter_proxy_model_->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
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

void View::on_metadata_object_changed(MetadataObject* obj,
	MetadataValueType value_type)
{
	// Check if we need to update the model's data range. We only work on the
	// end sample value because the start sample value is updated first and
	// we don't want to update the model twice
	if ((view_mode_selector_->currentIndex() == ViewModeVisible) &&
		(obj->type() == MetadataObjMainViewRange) &&
		(value_type == MetadataValueEndSample)) {

		int64_t start_sample = obj->value(MetadataValueStartSample).toLongLong();
		int64_t end_sample = obj->value(MetadataValueEndSample).toLongLong();

		filter_proxy_model_->set_sample_range(max((int64_t)0, start_sample),
			max((int64_t)0, end_sample));
	}

	if (obj->type() == MetadataObjMousePos) {
		QModelIndex first_visible_idx =
			filter_proxy_model_->mapToSource(filter_proxy_model_->index(0, 0));
		QModelIndex last_visible_idx =
			filter_proxy_model_->mapToSource(filter_proxy_model_->index(filter_proxy_model_->rowCount() - 1, 0));

		if (first_visible_idx.isValid()) {
			const QModelIndex first_highlighted_idx =
				model_->update_highlighted_rows(first_visible_idx, last_visible_idx,
					obj->value(MetadataValueStartSample).toLongLong());

			if (view_mode_selector_->currentIndex() == ViewModeVisible) {
				const QModelIndex idx = filter_proxy_model_->mapFromSource(first_highlighted_idx);
				table_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
			}

			// Force repaint, otherwise the table doesn't immediately update for some reason
			table_view_->viewport()->update();
		}
	}
}

void View::perform_delayed_view_update()
{
	update_data();
}


} // namespace tabular_decoder
} // namespace views
} // namespace pv
