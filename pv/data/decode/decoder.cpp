/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <cassert>

#include <QDebug>

#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrokdecode/libsigrokdecode.h>

#include "decoder.hpp"

#include <pv/data/signalbase.hpp>
#include <pv/data/decodesignal.hpp>

using std::map;
using std::string;

namespace pv {
namespace data {
namespace decode {

AnnotationClass::AnnotationClass(size_t _id, char* _name, char* _description, Row* _row) :
	id(_id),
	name(_name),
	description(_description),
	row(_row),
	visible_(true)
{
}

bool AnnotationClass::visible() const
{
	return visible_;
}

void AnnotationClass::set_visible(bool visible)
{
	visible_ = visible;

	visibility_changed();
}


Decoder::Decoder(const srd_decoder *const dec, uint8_t stack_level) :
	srd_decoder_(dec),
	stack_level_(stack_level),
	visible_(true),
	decoder_inst_(nullptr)
{
	// Query the annotation output classes
	uint32_t i = 0;
	for (GSList *l = dec->annotations; l; l = l->next) {
		char **ann_class = (char**)l->data;
		char *name = ann_class[0];
		char *desc = ann_class[1];
		ann_classes_.emplace_back(i++, name, desc, nullptr);

		connect(&(ann_classes_.back()), SIGNAL(visibility_changed()),
			this, SLOT(on_class_visibility_changed()));
	}

	// Query the binary output classes
	i = 0;
	for (GSList *l = dec->binary; l; l = l->next) {
		char **bin_class = (char**)l->data;
		char *name = bin_class[0];
		char *desc = bin_class[1];
		bin_classes_.push_back({i++, name, desc});
	}

	// Query the annotation rows and reference them by the classes that use them
	uint32_t row_count = 0;
	for (const GSList *rl = srd_decoder_->annotation_rows; rl; rl = rl->next)
		row_count++;

	i = 0;
	for (const GSList *rl = srd_decoder_->annotation_rows; rl; rl = rl->next) {
		const srd_decoder_annotation_row *const srd_row = (srd_decoder_annotation_row *)rl->data;
		assert(srd_row);
		rows_.emplace_back(i++, this, srd_row);

		// FIXME PV can crash from .at() if a PD's ann classes are defined incorrectly
		for (const GSList *cl = srd_row->ann_classes; cl; cl = cl->next)
			ann_classes_.at((size_t)cl->data).row = &(rows_.back());

		connect(&(rows_.back()), SIGNAL(visibility_changed()),
			this, SLOT(on_row_visibility_changed()));
	}

	if (rows_.empty()) {
		// Make sure there is a row for PDs without row declarations
		rows_.emplace_back(0, this);

		for (AnnotationClass& c : ann_classes_)
			c.row = &(rows_.back());
	}
}

Decoder::~Decoder()
{
	for (auto& option : options_)
		g_variant_unref(option.second);
}

const srd_decoder* Decoder::get_srd_decoder() const
{
	return srd_decoder_;
}

uint8_t Decoder::get_stack_level() const
{
	return stack_level_;
}

const char* Decoder::name() const
{
	return srd_decoder_->name;
}

bool Decoder::visible() const
{
	return visible_;
}

void Decoder::set_visible(bool visible)
{
	visible_ = visible;

	annotation_visibility_changed();
}

const vector<DecodeChannel*>& Decoder::channels() const
{
	return channels_;
}

void Decoder::set_channels(vector<DecodeChannel*> channels)
{
	channels_ = channels;
}

const map<string, GVariant*>& Decoder::options() const
{
	return options_;
}

void Decoder::set_option(const char *id, GVariant *value)
{
	assert(value);
	g_variant_ref(value);
	options_[id] = value;

	// If we have a decoder instance, apply option value immediately
	apply_all_options();
}

void Decoder::apply_all_options()
{
	if (decoder_inst_) {
		GHashTable *const opt_hash = g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

		for (const auto& option : options_) {
			GVariant *const value = option.second;
			g_variant_ref(value);
			g_hash_table_replace(opt_hash, (void*)g_strdup(
				option.first.c_str()), value);
		}

		srd_inst_option_set(decoder_inst_, opt_hash);
		g_hash_table_destroy(opt_hash);
	}
}

bool Decoder::have_required_channels() const
{
	for (DecodeChannel *ch : channels_)
		if (!ch->assigned_signal && !ch->is_optional)
			return false;

	return true;
}

srd_decoder_inst* Decoder::create_decoder_inst(srd_session *session)
{
	GHashTable *const opt_hash = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for (const auto& option : options_) {
		GVariant *const value = option.second;
		g_variant_ref(value);
		g_hash_table_replace(opt_hash, (void*)g_strdup(
			option.first.c_str()), value);
	}

	if (decoder_inst_)
		qDebug() << "WARNING: previous decoder instance" << decoder_inst_ << "exists";

	decoder_inst_ = srd_inst_new(session, srd_decoder_->id, opt_hash);
	g_hash_table_destroy(opt_hash);

	if (!decoder_inst_)
		return nullptr;

	// Setup the channels
	GArray *const init_pin_states = g_array_sized_new(false, true,
		sizeof(uint8_t), channels_.size());

	g_array_set_size(init_pin_states, channels_.size());

	GHashTable *const channels = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for (DecodeChannel *ch : channels_) {
		if (!ch->assigned_signal)
			continue;

		init_pin_states->data[ch->id] = ch->initial_pin_state;

		GVariant *const gvar = g_variant_new_int32(ch->bit_id);  // bit_id = bit position
		g_variant_ref_sink(gvar);
		// key is channel name (pdch->id), value is bit position in each sample (gvar)
		g_hash_table_insert(channels, ch->pdch_->id, gvar);
	}

	srd_inst_channel_set_all(decoder_inst_, channels);

	srd_inst_initial_pins_set_all(decoder_inst_, init_pin_states);
	g_array_free(init_pin_states, true);

	return decoder_inst_;
}

void Decoder::invalidate_decoder_inst()
{
	decoder_inst_ = nullptr;
}

vector<Row*> Decoder::get_rows()
{
	vector<Row*> result;

	for (Row& row : rows_)
		result.push_back(&row);

	return result;
}

Row* Decoder::get_row_by_id(size_t id)
{
	if (id > rows_.size())
		return nullptr;

	return &(rows_[id]);
}

vector<const AnnotationClass*> Decoder::ann_classes() const
{
	vector<const AnnotationClass*> result;

	for (const AnnotationClass& c : ann_classes_)
		result.push_back(&c);

	return result;
}

vector<AnnotationClass*> Decoder::ann_classes()
{
	vector<AnnotationClass*> result;

	for (AnnotationClass& c : ann_classes_)
		result.push_back(&c);

	return result;
}

AnnotationClass* Decoder::get_ann_class_by_id(size_t id)
{
	if (id >= ann_classes_.size())
		return nullptr;

	return &(ann_classes_[id]);
}

const AnnotationClass* Decoder::get_ann_class_by_id(size_t id) const
{
	if (id >= ann_classes_.size())
		return nullptr;

	return &(ann_classes_[id]);
}

uint32_t Decoder::get_binary_class_count() const
{
	return bin_classes_.size();
}

const DecodeBinaryClassInfo* Decoder::get_binary_class(uint32_t id) const
{
	return &(bin_classes_.at(id));
}

void Decoder::on_row_visibility_changed()
{
	annotation_visibility_changed();
}

void Decoder::on_class_visibility_changed()
{
	annotation_visibility_changed();
}

bool Decoder::has_logic_output() const
{
	return (srd_decoder_->logic_output_channels != nullptr);
}

const vector<DecoderLogicOutputChannel> Decoder::logic_output_channels() const
{
	vector<DecoderLogicOutputChannel> result;

	for (GSList *l = srd_decoder_->logic_output_channels; l; l = l->next) {
		const srd_decoder_logic_output_channel* ch_data =
			(srd_decoder_logic_output_channel*)l->data;

		result.emplace_back(QString::fromUtf8(ch_data->id),
			QString::fromUtf8(ch_data->desc));
	}

	return result;
}

}  // namespace decode
}  // namespace data
}  // namespace pv
