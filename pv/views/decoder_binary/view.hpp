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

#ifndef PULSEVIEW_PV_VIEWS_DECODER_BINARY_VIEW_HPP
#define PULSEVIEW_PV_VIEWS_DECODER_BINARY_VIEW_HPP

#include <QAction>
#include <QComboBox>
#include <QStackedWidget>
#include <QToolButton>

#include "pv/metadata_obj.hpp"
#include "pv/views/viewbase.hpp"
#include "pv/data/decodesignal.hpp"

#include "QHexView.hpp"

namespace pv {

class Session;

namespace views {

namespace decoder_binary {

// When adding an entry here, don't forget to update SaveTypeNames as well
enum SaveType {
	SaveTypeBinary,
	SaveTypeHexDumpPlain,
	SaveTypeHexDumpWithOffset,
	SaveTypeHexDumpComplete,
	SaveTypeCount  // Indicates how many save types there are, must always be last
};

extern const char* SaveTypeNames[SaveTypeCount];


class View : public ViewBase, public MetadataObjObserverInterface
{
	Q_OBJECT

public:
	explicit View(Session &session, bool is_main_view=false, QMainWindow *parent = nullptr);
	~View();

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
	void save_data_as_hex_dump(bool with_offset=false, bool with_ascii=false) const;

private Q_SLOTS:
	void on_selected_decoder_changed(int index);
	void on_selected_class_changed(int index);
	void on_signal_name_changed(const QString &name);
	void on_new_binary_data(unsigned int segment_id, void* decoder, unsigned int bin_class_id);

	void on_decoder_stacked(void* decoder);
	void on_decoder_removed(void* decoder);

	void on_actionSave_triggered(QAction* action = nullptr);

	virtual void on_metadata_object_changed(MetadataObject* obj,
		MetadataValueType value_type);

	virtual void perform_delayed_view_update();

private:
	QWidget* parent_;

	QComboBox *decoder_selector_, *format_selector_, *class_selector_;
	QStackedWidget *stacked_widget_;
	QHexView *hex_view_;

	QToolButton* save_button_;
	QAction* save_action_;

	data::DecodeSignal *signal_;
	const data::decode::Decoder *decoder_;
	uint32_t bin_class_id_;
	bool binary_data_exists_;
};

} // namespace decoder_binary
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_DECODER_BINARY_VIEW_HPP
