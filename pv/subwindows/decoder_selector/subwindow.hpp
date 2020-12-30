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

#ifndef PULSEVIEW_PV_SUBWINDOWS_DECODERSELECTOR_SUBWINDOW_HPP
#define PULSEVIEW_PV_SUBWINDOWS_DECODERSELECTOR_SUBWINDOW_HPP

#include <vector>

#include <QAbstractItemModel>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTreeView>

#include "pv/subwindows/subwindowbase.hpp"

using std::shared_ptr;

namespace pv {
namespace subwindows {
namespace decoder_selector {

class DecoderCollectionItem
{
public:
	DecoderCollectionItem(const vector<QVariant>& data,
		shared_ptr<DecoderCollectionItem> parent = nullptr);

	void appendSubItem(shared_ptr<DecoderCollectionItem> item);

	shared_ptr<DecoderCollectionItem> subItem(int row) const;
	shared_ptr<DecoderCollectionItem> parent() const;
	shared_ptr<DecoderCollectionItem> findSubItem(const QVariant& value, int column);

	int subItemCount() const;
	int columnCount() const;
	int row() const;
	QVariant data(int column) const;

private:
	vector< shared_ptr<DecoderCollectionItem> > subItems_;
	vector<QVariant> data_;
	shared_ptr<DecoderCollectionItem> parent_;
};


class DecoderCollectionModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	DecoderCollectionModel(QObject* parent = nullptr);

	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;

	QVariant headerData(int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
		const QModelIndex& parent_idx = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex& index) const override;

	int rowCount(const QModelIndex& parent_idx = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent_idx = QModelIndex()) const override;

private:
	shared_ptr<DecoderCollectionItem> root_;
};


class QCustomSortFilterProxyModel : public QSortFilterProxyModel
{
protected:
	bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
};

class QCustomTreeView : public QTreeView
{
	Q_OBJECT

public:
	void currentChanged(const QModelIndex& current, const QModelIndex& previous);

Q_SIGNALS:
	void current_changed(const QModelIndex& current);
};

class SubWindow : public SubWindowBase
{
	Q_OBJECT

public:
	explicit SubWindow(Session &session, QWidget *parent = nullptr);

	bool has_toolbar() const;
	QToolBar* create_toolbar(QWidget *parent) const;

	int minimum_width() const;

	/**
	 * Returns a list of input types that a given protocol decoder requires
	 * ("logic", "uart", etc.)
	 */
	vector<const char*> get_decoder_inputs(const srd_decoder* d) const;

	/**
	 * Returns a list of protocol decoder IDs which provide a given output
	 * ("uart", "spi", etc.)
	 */
	vector<const srd_decoder*> get_decoders_providing(const char* output) const;

Q_SIGNALS:
	void new_decoders_selected(vector<const srd_decoder*> decoders);

public Q_SLOTS:
	void on_item_changed(const QModelIndex& index);
	void on_item_activated(const QModelIndex& index);

	void on_filter_changed(const QString& text);
	void on_filter_return_pressed();

private:
	QSplitter* splitter_;
	QCustomTreeView* tree_view_;
	QWidget* info_box_;
	QLabel* info_label_header_;
	QLabel* info_label_body_;
	QLabel* info_label_footer_;
	DecoderCollectionModel* model_;
	QCustomSortFilterProxyModel* sort_filter_model_;
};

} // namespace decoder_selector
} // namespace subwindows
} // namespace pv

#endif // PULSEVIEW_PV_SUBWINDOWS_DECODERSELECTOR_SUBWINDOW_HPP

