/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2018 Soeren Apel <soeren@apelpie.net>
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

#include <algorithm>

#include <QDebug>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "pv/session.hpp"
#include "pv/subwindows/decoder_selector/subwindow.hpp"

using std::reverse;
using std::shared_ptr;

namespace pv {
namespace subwindows {
namespace decoder_selector {


bool QCustomSortFilterProxyModel::filterAcceptsRow(int source_row,
	const QModelIndex& source_parent) const
{
	// Search model recursively

	if (QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
		return true;

	const QModelIndex index = sourceModel()->index(source_row, 0, source_parent);

	for (int i = 0; i < sourceModel()->rowCount(index); i++)
		if (filterAcceptsRow(i, index))
			return true;

	return false;
}


void QCustomTreeView::currentChanged(const QModelIndex& current,
	const QModelIndex& previous)
{
	QTreeView::currentChanged(current, previous);
	currentChanged(current);
}


SubWindow::SubWindow(Session& session, QWidget* parent) :
	SubWindowBase(session, parent),
	splitter_(new QSplitter()),
	tree_view_(new QCustomTreeView()),
	info_box_(new QWidget()),
	info_label_header_(new QLabel()),
	info_label_body_(new QLabel()),
	info_label_footer_(new QLabel()),
	model_(new DecoderCollectionModel()),
	sort_filter_model_(new QCustomSortFilterProxyModel())
{
	QVBoxLayout* root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);
	root_layout->addWidget(splitter_);

	QWidget* upper_container = new QWidget();
	QVBoxLayout* upper_layout = new QVBoxLayout(upper_container);
	upper_layout->setContentsMargins(0, 5, 0, 0);
	QLineEdit* filter = new QLineEdit();
	upper_layout->addWidget(filter);
	upper_layout->addWidget(tree_view_);

	splitter_->setOrientation(Qt::Vertical);
	splitter_->addWidget(upper_container);
	splitter_->addWidget(info_box_);

	const QIcon filter_icon(QIcon::fromTheme("search",
		QIcon(":/icons/search.svg")));
	filter->setClearButtonEnabled(true);
	filter->addAction(filter_icon, QLineEdit::LeadingPosition);

	sort_filter_model_->setSourceModel(model_);
	sort_filter_model_->setFilterCaseSensitivity(Qt::CaseInsensitive);

	tree_view_->setModel(sort_filter_model_);
	tree_view_->setRootIsDecorated(true);
	tree_view_->setSortingEnabled(true);
	tree_view_->sortByColumn(0, Qt::AscendingOrder);

	// Hide the columns that hold the detailed item information
	tree_view_->hideColumn(2);  // ID

	info_box_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QVBoxLayout* info_box_layout = new QVBoxLayout(info_box_);
	info_box_layout->addWidget(info_label_header_);
	info_box_layout->addWidget(info_label_body_);
	info_box_layout->addWidget(info_label_footer_);
	info_label_body_->setWordWrap(true);
	info_label_body_->setText(tr("Select a decoder to see its description here."));

	connect(filter, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_filter_changed(const QString&)));

	connect(tree_view_, SIGNAL(currentChanged(const QModelIndex&)),
		this, SLOT(on_item_changed(const QModelIndex&)));
	connect(tree_view_, SIGNAL(activated(const QModelIndex&)),
		this, SLOT(on_item_activated(const QModelIndex&)));

	connect(this, SIGNAL(new_decoders_selected(vector<const srd_decoder*>)),
		&session, SLOT(on_new_decoders_selected(vector<const srd_decoder*>)));
}

bool SubWindow::has_toolbar() const
{
	return true;
}

QToolBar* SubWindow::create_toolbar(QWidget *parent) const
{
	QToolBar* toolbar = new QToolBar(parent);

	return toolbar;
}

const srd_decoder* SubWindow::get_srd_decoder_from_id(QString id) const
{
	const srd_decoder* ret_val = nullptr;

	for (GSList* li = (GSList*)srd_decoder_list(); li; li = li->next) {
		const srd_decoder* d = (srd_decoder*)li->data;
		assert(d);

		if (QString::fromUtf8(d->id) == id)
			ret_val = d;
	}

	return ret_val;
}

vector<const char*> SubWindow::decoder_inputs(const srd_decoder* d) const
{
	vector<const char*> ret_val;

	for (GSList* li = d->inputs; li; li = li->next)
		ret_val.push_back((const char*)li->data);

	return ret_val;
}

vector<const srd_decoder*> SubWindow::decoders_providing(const char* output) const
{
	vector<const srd_decoder*> ret_val;

	for (GSList* li = (GSList*)srd_decoder_list(); li; li = li->next) {
		const srd_decoder* d = (srd_decoder*)li->data;
		assert(d);

		if (!d->outputs)
			continue;

		// TODO For now we ignore that d->outputs is actually a list
		if (strncmp((char*)(d->outputs->data), output, strlen(output)) == 0)
			ret_val.push_back(d);
	}

	return ret_val;
}

void SubWindow::on_item_changed(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	QModelIndex id_index = index.model()->index(index.row(), 2, index.parent());
	QString decoder_name = index.model()->data(id_index, Qt::DisplayRole).toString();

	if (decoder_name.isEmpty())
		return;

	const srd_decoder* d = get_srd_decoder_from_id(decoder_name);

	const QString id = QString::fromUtf8(d->id);
	const QString longname = QString::fromUtf8(d->longname);
	const QString desc = QString::fromUtf8(d->desc);
	const QString doc = QString::fromUtf8(srd_decoder_doc_get(d));

	QString tags;
	for (GSList* li = (GSList*)d->tags; li; li = li->next) {
		QString s = (li == (GSList*)d->tags) ?
			tr((char*)li->data) :
			QString(tr(", %1")).arg(tr((char*)li->data));
		tags.append(s);
	}

	info_label_header_->setText(QString("<span style='font-size:large;font-weight:bold'>%1 (%2)</span><br>%3")
		.arg(longname, id, desc));
	info_label_body_->setText(doc);
	info_label_footer_->setText(tr("<p align='right'>Tags: %1</p>").arg(tags));
}

void SubWindow::on_item_activated(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	QModelIndex id_index = index.model()->index(index.row(), 2, index.parent());
	QString decoder_name = index.model()->data(id_index, Qt::DisplayRole).toString();

	const srd_decoder* chosen_decoder = get_srd_decoder_from_id(decoder_name);
	if (chosen_decoder == nullptr)
		return;

	vector<const srd_decoder*> decoders;
	decoders.push_back(chosen_decoder);

	// If the decoder only depends on logic inputs, we add it and are done
	vector<const char*> inputs = decoder_inputs(decoders.front());
	if (inputs.size() == 0) {
		qWarning() << "Protocol decoder" << decoder_name << "cannot have 0 inputs!";
		return;
	}

	if (strncmp(inputs.at(0), "logic", 5) == 0) {
		new_decoders_selected(decoders);
		return;
	}

	// Check if we can automatically fulfill the stacking requirements
	while (strncmp(inputs.at(0), "logic", 5) != 0) {
		vector<const srd_decoder*> prov_decoders = decoders_providing(inputs.at(0));

		if (prov_decoders.size() == 0) {
			// Emit warning and add the stack that we could gather so far
			qWarning() << "Protocol decoder" << QString::fromUtf8(decoders.back()->id) \
				<< "has input that no other decoder provides:" << QString::fromUtf8(inputs.at(0));
			break;
		}

		if (prov_decoders.size() == 1) {
			decoders.push_back(prov_decoders.front());
		} else {
			// Let user decide which one to use
			QString caption = QString(tr("Protocol decoder <b>%1</b> requires input type <b>%2</b> " \
				"which several decoders provide.<br>Choose which one to use:<br>"))
					.arg(QString::fromUtf8(decoders.back()->id), QString::fromUtf8(inputs.at(0)));

			QStringList items;
			for (const srd_decoder* d : prov_decoders)
				items << QString::fromUtf8(d->id) + " (" + QString::fromUtf8(d->longname) + ")";
			bool ok_clicked;
			QString item = QInputDialog::getItem(this, tr("Choose Decoder"),
				tr(caption.toUtf8()), items, 0, false, &ok_clicked);

			if ((!ok_clicked) || (item.isEmpty()))
				return;

			QString d = item.section(' ', 0, 0);
			decoders.push_back(get_srd_decoder_from_id(d));
		}

		inputs = decoder_inputs(decoders.back());
	}

	// Reverse decoder list and add the stack
	reverse(decoders.begin(), decoders.end());
	new_decoders_selected(decoders);
}

void SubWindow::on_filter_changed(const QString& text)
{
	sort_filter_model_->setFilterFixedString(text);
}

} // namespace decoder_selector
} // namespace subwindows
} // namespace pv
