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

#include <limits>

#include <QDebug>

#include "mathsignal.hpp"

#include <pv/globalsettings.hpp>
#include <pv/session.hpp>
#include <pv/data/analogsegment.hpp>
#include <pv/data/signalbase.hpp>

using std::dynamic_pointer_cast;
using std::make_shared;
using std::min;
using std::unique_lock;

namespace pv {
namespace data {

const int64_t MathSignal::ChunkLength = 256 * 1024;


MathSignal::MathSignal(pv::Session &session) :
	SignalBase(nullptr, SignalBase::MathChannel),
	session_(session),
	use_custom_sample_rate_(false),
	use_custom_sample_count_(false),
	expression_(""),
	error_message_(""),
	exprtk_symbol_table_(nullptr),
	exprtk_expression_(nullptr),
	exprtk_parser_(nullptr)
{
	set_name(QString(tr("Math%1")).arg(session_.get_next_signal_index(MathChannel)));

	shared_ptr<Analog> data(new data::Analog());
	set_data(data);

	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(on_capture_state_changed(int)));
	connect(&session_, SIGNAL(data_received()),
		this, SLOT(on_data_received()));

	expression_ = "sin(2 * pi * t) + cos(t / 2 * pi)";
}

MathSignal::~MathSignal()
{
	reset_generation();
}

void MathSignal::save_settings(QSettings &settings) const
{
	settings.setValue("expression", expression_);

	settings.setValue("custom_sample_rate", (qulonglong)custom_sample_rate_);
	settings.setValue("custom_sample_count", (qulonglong)custom_sample_count_);
	settings.setValue("use_custom_sample_rate", use_custom_sample_rate_);
	settings.setValue("use_custom_sample_count", use_custom_sample_count_);
}

void MathSignal::restore_settings(QSettings &settings)
{
	if (settings.contains("expression"))
		expression_ = settings.value("expression").toString();

	if (settings.contains("custom_sample_rate"))
		custom_sample_rate_ = settings.value("custom_sample_rate").toULongLong();

	if (settings.contains("custom_sample_count"))
		custom_sample_count_ = settings.value("custom_sample_count").toULongLong();

	if (settings.contains("use_custom_sample_rate"))
		use_custom_sample_rate_ = settings.value("use_custom_sample_rate").toBool();

	if (settings.contains("use_custom_sample_count"))
		use_custom_sample_count_ = settings.value("use_custom_sample_count").toBool();
}

QString MathSignal::error_message() const
{
	return error_message_;
}

QString MathSignal::get_expression() const
{
	return expression_;
}

void MathSignal::set_expression(QString expression)
{
	expression_ = expression;

	begin_generation();
}

void MathSignal::set_error_message(QString msg)
{
	error_message_ = msg;
	// TODO Emulate noquote()
	qDebug().nospace() << name() << ": " << msg << "(Expression: '" << expression_ << "')";
}

uint64_t MathSignal::get_working_sample_count(uint32_t segment_id) const
{
	// The working sample count is the highest sample number for
	// which all used signals have data available, so go through all
	// channels and use the lowest overall sample count of the segment

	int64_t result = std::numeric_limits<int64_t>::max();

	if (use_custom_sample_count_)
		// A custom sample count implies that only one segment will be created
		result = (segment_id == 0) ? custom_sample_count_ : 0;
	else {
		if (input_signals_.size() > 0) {
			for (const shared_ptr<SignalBase> &sb : input_signals_) {
				shared_ptr<Analog> a = sb->analog_data();
				const uint32_t last_segment = (a->analog_segments().size() - 1);
				if (segment_id > last_segment)
					continue;
				const shared_ptr<AnalogSegment> segment = a->analog_segments()[segment_id];
				result = min(result, (int64_t)segment->get_sample_count());
			}
		} else
			result = session_.get_segment_sample_count(segment_id);
	}

	return result;
}

void MathSignal::reset_generation()
{
	if (gen_thread_.joinable()) {
		gen_interrupt_ = true;
		gen_input_cond_.notify_one();
		gen_thread_.join();
	}

	data_->clear();

	if (exprtk_symbol_table_) {
		delete exprtk_symbol_table_;
		exprtk_symbol_table_ = nullptr;
	}

	if (exprtk_expression_) {
		delete exprtk_expression_;
		exprtk_expression_ = nullptr;
	}

	if (exprtk_parser_) {
		delete exprtk_parser_;
		exprtk_parser_ = nullptr;
	}

	if (!error_message_.isEmpty()) {
		error_message_ = QString();
		// TODO Emulate noquote()
		qDebug().nospace() << name() << ": Error cleared";
	}
}

void MathSignal::begin_generation()
{
	reset_generation();

	if (expression_.isEmpty()) {
		set_error_message(tr("No expression defined, nothing to do"));
		return;
	}

	exprtk_symbol_table_ = new exprtk::symbol_table<double>();
	exprtk_symbol_table_->add_variable("t", exprtk_current_time_);
	exprtk_symbol_table_->add_variable("s", exprtk_current_sample_);
	exprtk_symbol_table_->add_constants();

	exprtk_expression_ = new exprtk::expression<double>();
	exprtk_expression_->register_symbol_table(*exprtk_symbol_table_);

	exprtk_parser_ = new exprtk::parser<double>();
	if (!exprtk_parser_->compile(expression_.toStdString(), *exprtk_expression_)) {
		error_message_ = set_error_message(tr("Error in expression"));
	} else {
		gen_interrupt_ = false;
		gen_thread_ = std::thread(&MathSignal::generation_proc, this);
	}
}

void MathSignal::generate_samples(uint32_t segment_id, const uint64_t start_sample,
	const int64_t sample_count)
{
	shared_ptr<Analog> analog = dynamic_pointer_cast<Analog>(data_);
	shared_ptr<AnalogSegment> segment = analog->analog_segments().at(segment_id);

	const double sample_rate = data_->get_samplerate();

	exprtk_current_sample_ = start_sample;

	float *sample_data = new float[sample_count];

	for (int64_t i = 0; i < sample_count; i++) {
		exprtk_current_sample_ += 1;
		exprtk_current_time_ = exprtk_current_sample_ / sample_rate;
		double value = exprtk_expression_->value();
		sample_data[i] = value;
	}

	segment->append_interleaved_samples(sample_data, sample_count, 1);

	delete[] sample_data;
}

void MathSignal::generation_proc()
{
	uint32_t segment_id = 0;

	// Don't do anything until we have a valid sample rate
	do {
		if (use_custom_sample_rate_)
			data_->set_samplerate(custom_sample_rate_);
		else
			data_->set_samplerate(session_.get_samplerate());

		if (data_->get_samplerate() == 1) {
			unique_lock<mutex> gen_input_lock(input_mutex_);
			gen_input_cond_.wait(gen_input_lock);
		}
	} while ((!gen_interrupt_) && (data_->get_samplerate() == 1));

	if (gen_interrupt_)
		return;

	shared_ptr<Analog> analog = dynamic_pointer_cast<Analog>(data_);

	// Create initial analog segment
	shared_ptr<AnalogSegment> output_segment =
		make_shared<AnalogSegment>(*analog.get(), segment_id, analog->get_samplerate());
	analog->push_segment(output_segment);

	// Create analog samples
	do {
		const uint64_t input_sample_count = get_working_sample_count(segment_id);
		const uint64_t output_sample_count = output_segment->get_sample_count();

		const uint64_t samples_to_process =
			(input_sample_count > output_sample_count) ?
			(input_sample_count - output_sample_count) : 0;

		// Process the samples if necessary...
		if (samples_to_process > 0) {
			const uint64_t chunk_sample_count = ChunkLength;

			uint64_t processed_samples = 0;
			do {
				const uint64_t start_sample = output_sample_count + processed_samples;
				const uint64_t sample_count =
					min(samples_to_process - processed_samples,	chunk_sample_count);

				generate_samples(segment_id, start_sample, sample_count);
				processed_samples += sample_count;

				// Notify consumers of this signal's data
				// TODO Does this work when a conversion is active?
				samples_added(segment_id, start_sample, start_sample + processed_samples);
			} while (!gen_interrupt_ && (processed_samples < samples_to_process));
		}

		if (samples_to_process == 0) {
			if (segment_id < session_.get_highest_segment_id()) {
				// Process next segment
				segment_id++;

				output_segment =
					make_shared<AnalogSegment>(*analog.get(), segment_id, analog->get_samplerate());
				analog->push_segment(output_segment);
			} else {
				// All segments have been processed, wait for more input
				unique_lock<mutex> gen_input_lock(input_mutex_);
				gen_input_cond_.wait(gen_input_lock);
			}
		}

	} while (!gen_interrupt_);
}

void MathSignal::on_capture_state_changed(int state)
{
	if (state == Session::Running)
		begin_generation();
}

void MathSignal::on_data_received()
{
	gen_input_cond_.notify_one();
}

} // namespace data
} // namespace pv
