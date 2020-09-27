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

#include <extdef.h>
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

#define MATH_ERR_NONE           0
#define MATH_ERR_EMPTY_EXPR     1
#define MATH_ERR_EXPRESSION     2
#define MATH_ERR_INVALID_SIGNAL 3
#define MATH_ERR_ENABLE         4

const int64_t MathSignal::ChunkLength = 256 * 1024;


template<typename T>
struct fnc_sample : public exprtk::igeneric_function<T>
{
	typedef typename exprtk::igeneric_function<T>::parameter_list_t parameter_list_t;
	typedef typename exprtk::igeneric_function<T>::generic_type generic_type;
	typedef typename generic_type::scalar_view scalar_t;
	typedef typename generic_type::string_view string_t;

	fnc_sample(MathSignal& owner) :
		exprtk::igeneric_function<T>("ST"),  // Require channel name and sample number
		owner_(owner),
		sig_data(nullptr)
	{
	}

	T operator()(parameter_list_t parameters)
	{
		const string_t exprtk_sig_name = string_t(parameters[0]);
		const scalar_t exprtk_sample_num = scalar_t(parameters[1]);

		const std::string str_sig_name = to_str(exprtk_sig_name);
		const double sample_num = exprtk_sample_num();

		if (sample_num < 0)
			return 0;

		if (!sig_data)
			sig_data = owner_.signal_from_name(str_sig_name);

		if (!sig_data)
			// There doesn't actually exist a signal with that name
			return 0;

		owner_.update_signal_sample(sig_data, current_segment, sample_num);

		return T(sig_data->sample_value);
	}

	MathSignal& owner_;
	uint32_t current_segment;
	signal_data* sig_data;
};


MathSignal::MathSignal(pv::Session &session) :
	SignalBase(nullptr, SignalBase::MathChannel),
	session_(session),
	use_custom_sample_rate_(false),
	use_custom_sample_count_(false),
	expression_(""),
	error_type_(MATH_ERR_NONE),
	exprtk_unknown_symbol_table_(nullptr),
	exprtk_symbol_table_(nullptr),
	exprtk_expression_(nullptr),
	exprtk_parser_(nullptr),
	fnc_sample_(nullptr)
{
	uint32_t sig_idx = session_.get_next_signal_index(MathChannel);
	set_name(QString(tr("Math%1")).arg(sig_idx));
	set_color(AnalogSignalColors[(sig_idx - 1) % countof(AnalogSignalColors)]);

	set_data(std::make_shared<data::Analog>());

	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(on_capture_state_changed(int)));
}

MathSignal::~MathSignal()
{
	reset_generation();
}

void MathSignal::save_settings(QSettings &settings) const
{
	SignalBase::save_settings(settings);

	settings.setValue("expression", expression_);

	settings.setValue("custom_sample_rate", (qulonglong)custom_sample_rate_);
	settings.setValue("custom_sample_count", (qulonglong)custom_sample_count_);
	settings.setValue("use_custom_sample_rate", use_custom_sample_rate_);
	settings.setValue("use_custom_sample_count", use_custom_sample_count_);
}

void MathSignal::restore_settings(QSettings &settings)
{
	SignalBase::restore_settings(settings);

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

QString MathSignal::get_expression() const
{
	return expression_;
}

void MathSignal::set_expression(QString expression)
{
	expression_ = expression;

	begin_generation();
}

void MathSignal::set_error(uint8_t type, QString msg)
{
	error_type_ = type;
	error_message_ = msg;
	// TODO Emulate noquote()
	qDebug().nospace() << name() << ": " << msg << "(Expression: '" + expression_ + "')";

	error_message_changed(msg);
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
			for (auto input_signal : input_signals_) {
				const shared_ptr<SignalBase>& sb = input_signal.second.sb;

				shared_ptr<Analog> a = sb->analog_data();
				auto analog_segments = a->analog_segments();

				if (analog_segments.size() == 0) {
					result = 0;
					continue;
				}

				const uint32_t highest_segment_id = (analog_segments.size() - 1);
				if (segment_id > highest_segment_id)
					continue;

				const shared_ptr<AnalogSegment> segment = analog_segments.at(segment_id);
				result = min(result, (int64_t)segment->get_sample_count());
			}
		} else
			result = session_.get_segment_sample_count(segment_id);
	}

	return result;
}

void MathSignal::update_completeness(uint32_t segment_id, uint64_t output_sample_count)
{
	bool output_complete = true;

	if (input_signals_.size() > 0) {
		for (auto input_signal : input_signals_) {
			const shared_ptr<SignalBase>& sb = input_signal.second.sb;

			shared_ptr<Analog> a = sb->analog_data();
			auto analog_segments = a->analog_segments();

			if (analog_segments.size() == 0) {
				output_complete = false;
				continue;
			}

			const uint32_t highest_segment_id = (analog_segments.size() - 1);
			if (segment_id > highest_segment_id) {
				output_complete = false;
				continue;
			}

			const shared_ptr<AnalogSegment> segment = analog_segments.at(segment_id);
			if (!segment->is_complete()) {
				output_complete = false;
				continue;
			}

			if (output_sample_count < segment->get_sample_count())
				output_complete = false;
		}
	} else {
		// We're done when we generated as many samples as the stopped session is long
		if ((session_.get_capture_state() != Session::Stopped) ||
			(output_sample_count < session_.get_segment_sample_count(segment_id)))
			output_complete = false;
	}

	if (output_complete)
		analog_data()->analog_segments().at(segment_id)->set_complete();
}

void MathSignal::reset_generation()
{
	if (gen_thread_.joinable()) {
		gen_interrupt_ = true;
		gen_input_cond_.notify_one();
		gen_thread_.join();
	}

	data_->clear();
	input_signals_.clear();

	if (exprtk_parser_) {
		delete exprtk_parser_;
		exprtk_parser_ = nullptr;
	}

	if (exprtk_expression_) {
		delete exprtk_expression_;
		exprtk_expression_ = nullptr;
	}

	if (exprtk_symbol_table_) {
		delete exprtk_symbol_table_;
		exprtk_symbol_table_ = nullptr;
	}

	if (exprtk_unknown_symbol_table_) {
		delete exprtk_unknown_symbol_table_;
		exprtk_unknown_symbol_table_ = nullptr;
	}

	if (fnc_sample_) {
		delete fnc_sample_;
		fnc_sample_ = nullptr;
	}

	if (!error_message_.isEmpty()) {
		error_message_.clear();
		error_type_ = MATH_ERR_NONE;
		// TODO Emulate noquote()
		qDebug().nospace() << name() << ": Error cleared";
	}

	generation_chunk_size_ = ChunkLength;
}

void MathSignal::begin_generation()
{
	reset_generation();

	if (expression_.isEmpty()) {
		set_error(MATH_ERR_EMPTY_EXPR, tr("No expression defined, nothing to do"));
		return;
	}

	disconnect(this, SLOT(on_data_received()));
	disconnect(this, SLOT(on_enabled_changed()));

	fnc_sample_ = new fnc_sample<double>(*this);

	exprtk_unknown_symbol_table_ = new exprtk::symbol_table<double>();

	exprtk_symbol_table_ = new exprtk::symbol_table<double>();
	exprtk_symbol_table_->add_constant("T", 1 / session_.get_samplerate());
	exprtk_symbol_table_->add_function("sample", *fnc_sample_);
	exprtk_symbol_table_->add_variable("t", exprtk_current_time_);
	exprtk_symbol_table_->add_variable("s", exprtk_current_sample_);
	exprtk_symbol_table_->add_constants();

	exprtk_expression_ = new exprtk::expression<double>();
	exprtk_expression_->register_symbol_table(*exprtk_unknown_symbol_table_);
	exprtk_expression_->register_symbol_table(*exprtk_symbol_table_);

	exprtk_parser_ = new exprtk::parser<double>();
	exprtk_parser_->enable_unknown_symbol_resolver();

	if (!exprtk_parser_->compile(expression_.toStdString(), *exprtk_expression_)) {
		QString error_details;
		size_t error_count = exprtk_parser_->error_count();

		for (size_t i = 0; i < error_count; i++) {
			typedef exprtk::parser_error::type error_t;
			error_t error = exprtk_parser_->get_error(i);
			exprtk::parser_error::update_error(error, expression_.toStdString());

			QString error_detail = tr("%1 at line %2, column %3: %4");
			if ((error_count > 1) && (i < (error_count - 1)))
				error_detail += "\n";

			error_details += error_detail \
				.arg(exprtk::parser_error::to_str(error.mode).c_str()) \
				.arg(error.line_no) \
				.arg(error.column_no) \
				.arg(error.diagnostic.c_str());
		}
		set_error(MATH_ERR_EXPRESSION, error_details);
	} else {
		// Resolve unknown scalars to signals and add them to the input signal list
		vector<string> unknowns;
		exprtk_unknown_symbol_table_->get_variable_list(unknowns);
		for (string& unknown : unknowns) {
			signal_data* sig_data = signal_from_name(unknown);
			const shared_ptr<SignalBase> signal = (sig_data) ? (sig_data->sb) : nullptr;
			if (!signal || (!signal->analog_data())) {
				set_error(MATH_ERR_INVALID_SIGNAL, QString(tr("\"%1\" isn't a valid analog signal")) \
					.arg(QString::fromStdString(unknown)));
			} else
				sig_data->ref = &(exprtk_unknown_symbol_table_->variable_ref(unknown));
		}
	}

	QString disabled_signals;
	if (!all_input_signals_enabled(disabled_signals) && error_message_.isEmpty())
		set_error(MATH_ERR_ENABLE,
			tr("No data will be generated as %1 must be enabled").arg(disabled_signals));

	if (error_message_.isEmpty()) {
		// Connect to the session data notification if we have no input signals
		if (input_signals_.empty())
			connect(&session_, SIGNAL(data_received()),
				this, SLOT(on_data_received()));

		gen_interrupt_ = false;
		gen_thread_ = std::thread(&MathSignal::generation_proc, this);
	}
}

uint64_t MathSignal::generate_samples(uint32_t segment_id, const uint64_t start_sample,
	const int64_t sample_count)
{
	uint64_t count = 0;

	shared_ptr<Analog> analog = dynamic_pointer_cast<Analog>(data_);
	shared_ptr<AnalogSegment> segment = analog->analog_segments().at(segment_id);

	// Keep the math functions segment IDs in sync
	fnc_sample_->current_segment = segment_id;

	const double sample_rate = data_->get_samplerate();

	exprtk_current_sample_ = start_sample;

	float *sample_data = new float[sample_count];

	for (int64_t i = 0; i < sample_count; i++) {
		exprtk_current_time_ = exprtk_current_sample_ / sample_rate;

		for (auto& entry : input_signals_) {
			signal_data* sig_data  = &(entry.second);
			update_signal_sample(sig_data, segment_id, exprtk_current_sample_);
		}

		double value = exprtk_expression_->value();
		sample_data[i] = value;
		exprtk_current_sample_ += 1;
		count++;

		// If during the evaluation of the expression it was found that this
		// math signal itself is being accessed, the chunk size was reduced
		// to 1, which means we must stop after this sample we just generated
		if (generation_chunk_size_ == 1)
			break;
	}

	segment->append_interleaved_samples(sample_data, count, 1);

	delete[] sample_data;

	return count;
}

void MathSignal::generation_proc()
{
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

	uint32_t segment_id = 0;
	shared_ptr<Analog> analog = analog_data();

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
			uint64_t processed_samples = 0;
			do {
				const uint64_t start_sample = output_sample_count + processed_samples;
				uint64_t sample_count =
					min(samples_to_process - processed_samples,	generation_chunk_size_);

				sample_count = generate_samples(segment_id, start_sample, sample_count);
				processed_samples += sample_count;

				// Notify consumers of this signal's data
				samples_added(segment_id, start_sample, start_sample + processed_samples);
			} while (!gen_interrupt_ && (processed_samples < samples_to_process));
		}

		update_completeness(segment_id, output_sample_count);

		if (output_segment->is_complete() && (segment_id < session_.get_highest_segment_id())) {
				// Process next segment
				segment_id++;

				output_segment =
					make_shared<AnalogSegment>(*analog.get(), segment_id, analog->get_samplerate());
				analog->push_segment(output_segment);
		}

		if (!gen_interrupt_ && (samples_to_process == 0)) {
			// Wait for more input
			unique_lock<mutex> gen_input_lock(input_mutex_);
			gen_input_cond_.wait(gen_input_lock);
		}
	} while (!gen_interrupt_);
}

signal_data* MathSignal::signal_from_name(const std::string& name)
{
	// If the expression contains the math signal itself, we must add every sample to
	// the output segment immediately so that it can be accessed
	const QString sig_name = QString::fromStdString(name);
	if (sig_name == this->name())
		generation_chunk_size_ = 1;

	// Look up signal in the map and if it doesn't exist yet, add it for future use

	auto element = input_signals_.find(name);

	if (element != input_signals_.end()) {
		return &(element->second);
	} else {
		const vector< shared_ptr<SignalBase> > signalbases = session_.signalbases();

		for (const shared_ptr<SignalBase>& sb : signalbases)
			if (sb->name() == sig_name) {
				if (!sb->analog_data())
					continue;

				connect(sb->analog_data().get(), SIGNAL(samples_added(SharedPtrToSegment, uint64_t, uint64_t)),
					this, SLOT(on_data_received()));
				connect(sb->analog_data().get(), SIGNAL(segment_completed()),
					this, SLOT(on_data_received()));

				connect(sb.get(), SIGNAL(enabled_changed(bool)),
					this, SLOT(on_enabled_changed()));

				return &(input_signals_.insert({name, signal_data(sb)}).first->second);
			}
	}

	// If we reach this point, no valid signal was found with the supplied name
	if (error_type_ == MATH_ERR_NONE)
		set_error(MATH_ERR_INVALID_SIGNAL, QString(tr("\"%1\" isn't a valid analog signal")) \
			.arg(QString::fromStdString(name)));

	return nullptr;
}

void MathSignal::update_signal_sample(signal_data* sig_data, uint32_t segment_id, uint64_t sample_num)
{
	assert(sig_data);

	// Update the value only if a different sample is requested
	if (sig_data->sample_num == sample_num)
		return;

	assert(sig_data->sb);
	const shared_ptr<pv::data::Analog> analog = sig_data->sb->analog_data();
	assert(analog);

	assert(segment_id < analog->analog_segments().size());

	const shared_ptr<AnalogSegment> segment = analog->analog_segments().at(segment_id);

	sig_data->sample_num = sample_num;

	if (sample_num < segment->get_sample_count())
		sig_data->sample_value = segment->get_sample(sample_num);
	else
		sig_data->sample_value = 0;

	// We only have a reference if this signal is used as a scalar;
	// if it's used by a function, it's null
	if (sig_data->ref)
		*(sig_data->ref) = sig_data->sample_value;
}

bool MathSignal::all_input_signals_enabled(QString &disabled_signals) const
{
	bool all_enabled = true;

	disabled_signals.clear();

	for (auto input_signal : input_signals_) {
		const shared_ptr<SignalBase>& sb = input_signal.second.sb;

		if (!sb->enabled()) {
			all_enabled = false;
			disabled_signals += disabled_signals.isEmpty() ?
				sb->name() : ", " + sb->name();
		}
	}

	return all_enabled;
}

void MathSignal::on_capture_state_changed(int state)
{
	if (state == Session::Running)
		begin_generation();

	// Make sure we don't miss any input samples, just in case
	if (state == Session::Stopped)
		gen_input_cond_.notify_one();
}

void MathSignal::on_data_received()
{
	gen_input_cond_.notify_one();
}

void MathSignal::on_enabled_changed()
{
	QString disabled_signals;
	if (!all_input_signals_enabled(disabled_signals) &&
		((error_type_ == MATH_ERR_NONE) || (error_type_ == MATH_ERR_ENABLE)))
		set_error(MATH_ERR_ENABLE,
			tr("No data will be generated as %1 must be enabled").arg(disabled_signals));
	else if (disabled_signals.isEmpty() && (error_type_ == MATH_ERR_ENABLE)) {
		error_type_ = MATH_ERR_NONE;
		error_message_.clear();
	}
}

} // namespace data
} // namespace pv
