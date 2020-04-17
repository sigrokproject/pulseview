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

#ifndef PULSEVIEW_PV_VIEWS_TABULARDECODER_VIEW_HPP
#define PULSEVIEW_PV_VIEWS_TABULARDECODER_VIEW_HPP

#include <QAction>
#include <QComboBox>
#include <QTableView>
#include <QToolButton>

#include "pv/views/viewbase.hpp"
#include "pv/data/decodesignal.hpp"

namespace pv {
class Session;

namespace views {

namespace tabular_decoder {

class AnnotationCollectionModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	AnnotationCollectionModel(QObject* parent = nullptr);

	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;

	QVariant headerData(int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
		const QModelIndex& parent_idx = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex& index) const override;

	int rowCount(const QModelIndex& parent_idx = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent_idx = QModelIndex()) const override;

	void set_signal_and_segment(data::DecodeSignal* signal, uint32_t current_segment);

private:
	vector<QVariant> header_data_;
	const deque<const Annotation*>* all_annotations_;
	uint32_t prev_segment_;
	uint64_t prev_last_row_;
};


class QCustomTableView : public QTableView
{
	Q_OBJECT

public:
	QSize minimumSizeHint() const;
	QSize sizeHint() const;
};


class View : public ViewBase
{
	Q_OBJECT

public:
	explicit View(Session &session, bool is_main_view=false, QMainWindow *parent = nullptr);

	virtual ViewType get_type() const;

	/**
	 * Resets the view to its default state after construction. It does however
	 * not reset the signal bases or any other connections with the session.
	 */
	virtual void reset_view_state();

	virtual void clear_decode_signals();
	virtual void add_decode_signal(shared_ptr<data::DecodeSignal> signal);
	virtual void remove_decode_signal(shared_ptr<data::DecodeSignal> signal);

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

private:
	void reset_data();
	void update_data();

	void save_data() const;

private Q_SLOTS:
	void on_selected_decoder_changed(int index);
	void on_signal_name_changed(const QString &name);
	void on_new_annotations();

	void on_decoder_reset();
	void on_decoder_stacked(void* decoder);
	void on_decoder_removed(void* decoder);

	void on_actionSave_triggered(QAction* action = nullptr);

	virtual void perform_delayed_view_update();

private:
	QWidget* parent_;

	QComboBox* decoder_selector_;

	QToolButton* save_button_;
	QAction* save_action_;

	QCustomTableView* table_view_;

	AnnotationCollectionModel* model_;

	data::DecodeSignal* signal_;
	const data::decode::Decoder* decoder_;
	bool updating_data_;
};

} // namespace tabular_decoder
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TABULARDECODER_VIEW_HPP
