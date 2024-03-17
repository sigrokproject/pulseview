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

#ifndef PULSEVIEW_PV_DATA_MATHSIGNALBASE_HPP
#define PULSEVIEW_PV_DATA_MATHSIGNALBASE_HPP

#define exprtk_disable_rtl_io_file /* Potential security issue, doubt anyone would use those anyway */
#define exprtk_disable_rtl_vecops  /* Vector ops are rather useless for math channels */
#define exprtk_disable_caseinsensitivity /* So that we can have both 't' and 'T' */

#include <limits>
#include <extdef.h>

#include <QDebug>
#include <QString>

#include <pv/exprtk.hpp>
#include <pv/globalsettings.hpp>
#include <pv/session.hpp>
#include <pv/util.hpp>
#include <pv/data/signalbase.hpp>

using std::atomic;
using std::condition_variable;
using std::mutex;
using std::numeric_limits;
using std::shared_ptr;

using std::dynamic_pointer_cast;
using std::make_shared;
using std::min;
using std::unique_lock;

namespace pv {
class Session;

namespace data {

class SignalBase;

template<typename T, typename U>
struct fnc_sample;

struct signal_data {
	signal_data(const shared_ptr<SignalBase> _sb) :
		sb(_sb), sample_num(numeric_limits<uint64_t>::max()), sample_value(0), ref(nullptr)
	{}

	const shared_ptr<SignalBase> sb;
	uint64_t sample_num;
	double sample_value;
	double* ref;
};

template <typename T>
class MathSignalBase : public SignalBase
{
protected:
	static const int64_t ChunkLength = 256 * 1024;

public:
	MathSignalBase(pv::Session &session);
	virtual ~MathSignalBase();

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

	QString get_expression() const;
	void set_expression(QString expression);

protected:
	void set_error(uint8_t type, QString msg);

	/**
	 * Returns the number of samples that can be worked on,
	 * i.e. the number of samples where samples are available
	 * for all connected channels.
	 * If the math signal uses no input channels, this is the
	 * number of samples in the session.
	 */
	uint64_t get_working_sample_count(uint32_t segment_id) const;

	void update_completeness(uint32_t segment_id, uint64_t output_sample_count);

	void reset_generation();
	virtual void begin_generation();

	uint64_t generate_samples(uint32_t segment_id, const uint64_t start_sample,
		const int64_t sample_count);
	void generation_proc();

	signal_data* signal_from_name(const std::string& name);
	void update_signal_sample(signal_data* sig_data, uint32_t segment_id, uint64_t sample_num);

	bool all_input_signals_enabled(QString &disabled_signals) const;

protected:
	pv::Session &session_;

	uint64_t custom_sample_rate_;
	uint64_t custom_sample_count_;
	bool use_custom_sample_rate_, use_custom_sample_count_;
	uint64_t generation_chunk_size_;
	map<std::string, signal_data> input_signals_;

	QString expression_;

	uint8_t error_type_;

	mutable mutex input_mutex_;
	mutable condition_variable gen_input_cond_;

	std::thread gen_thread_;
	atomic<bool> gen_interrupt_;

	exprtk::symbol_table<double> *exprtk_unknown_symbol_table_, *exprtk_symbol_table_;
	exprtk::expression<double> *exprtk_expression_;
	exprtk::parser<double> *exprtk_parser_;
	double exprtk_current_time_, exprtk_current_sample_;

	fnc_sample<T, double>* fnc_sample_;

	// Give sig_sample access to the private helper functions
	friend struct fnc_sample<T, double>;
};

#define MATH_ERR_NONE           0
#define MATH_ERR_EMPTY_EXPR     1
#define MATH_ERR_EXPRESSION     2
#define MATH_ERR_INVALID_SIGNAL 3
#define MATH_ERR_ENABLE         4

template<typename T, typename U>
struct fnc_sample : public exprtk::igeneric_function<U>
{
	typedef typename exprtk::igeneric_function<U>::parameter_list_t parameter_list_t;
	typedef typename exprtk::igeneric_function<U>::generic_type generic_type;
	typedef typename generic_type::scalar_view scalar_t;
	typedef typename generic_type::string_view string_t;

	fnc_sample(MathSignalBase<T>& owner) :
		exprtk::igeneric_function<U>("ST"),  // Require channel name and sample number
		owner_(owner),
		sig_data(nullptr)
	{
	}

	U operator()(parameter_list_t parameters)
	{
		const string_t exprtk_sig_name = string_t(parameters[0]);
		const scalar_t exprtk_sample_num = scalar_t(parameters[1]);

		const std::string str_sig_name = to_str(exprtk_sig_name);
		const auto sample_num = exprtk_sample_num();

		if (sample_num < 0)
			return 0;

		if (!sig_data)
			sig_data = owner_.signal_from_name(str_sig_name);

		if (!sig_data)
			// There doesn't actually exist a signal with that name
			return 0;

		owner_.update_signal_sample(sig_data, current_segment, sample_num);

		return U(sig_data->sample_value);
	}

	MathSignalBase<T>& owner_;
	uint32_t current_segment;
	signal_data* sig_data;
};

template <typename T>
MathSignalBase<T>::MathSignalBase(pv::Session &session) :
	SignalBase(nullptr, T::math_channel_type),
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
	uint32_t sig_idx = session_.get_next_signal_index(T::math_channel_type);
	set_name(QString(tr("Math%1")).arg(sig_idx));
	set_color(T::SignalColors[(sig_idx - 1) % countof(T::SignalColors)]);

	set_data(std::make_shared<T>());

	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(on_capture_state_changed(int)));
}

template <typename T>
MathSignalBase<T>::~MathSignalBase()
{
	reset_generation();
}

template <typename T>
void MathSignalBase<T>::save_settings(QSettings &settings) const
{
	SignalBase::save_settings(settings);

	settings.setValue("expression", expression_);

	settings.setValue("custom_sample_rate", (qulonglong)custom_sample_rate_);
	settings.setValue("custom_sample_count", (qulonglong)custom_sample_count_);
	settings.setValue("use_custom_sample_rate", use_custom_sample_rate_);
	settings.setValue("use_custom_sample_count", use_custom_sample_count_);
}

template <typename T>
void MathSignalBase<T>::restore_settings(QSettings &settings)
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

template <typename T>
QString MathSignalBase<T>::get_expression() const
{
	return expression_;
}

template <typename T>
void MathSignalBase<T>::set_expression(QString expression)
{
	expression_ = expression;

	begin_generation();
}

template <typename T>
void MathSignalBase<T>::set_error(uint8_t type, QString msg)
{
	error_type_ = type;
	error_message_ = msg;
	// TODO Emulate noquote()
	qDebug().nospace() << name() << ": " << msg << "(Expression: '" + expression_ + "')";

	error_message_changed(msg);
}

template <typename T>
uint64_t MathSignalBase<T>::get_working_sample_count(uint32_t segment_id) const
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

				shared_ptr<T> a = sb->typed_data<T>();
				auto typed_segments = a->typed_segments();

				if (typed_segments.size() == 0) {
					result = 0;
					continue;
				}

				const uint32_t highest_segment_id = (typed_segments.size() - 1);
				if (segment_id > highest_segment_id)
					continue;

				const auto segment = typed_segments.at(segment_id);
				result = min(result, (int64_t)segment->get_sample_count());
			}
		} else
			result = session_.get_segment_sample_count(segment_id);
	}

	return result;
}

template <typename T>
void MathSignalBase<T>::update_completeness(uint32_t segment_id, uint64_t output_sample_count)
{
	bool output_complete = true;

	if (input_signals_.size() > 0) {
		for (auto input_signal : input_signals_) {
			const shared_ptr<SignalBase>& sb = input_signal.second.sb;

			auto a = sb->typed_data<T>();
			auto typed_segments = a->typed_segments();

			if (typed_segments.size() == 0) {
				output_complete = false;
				continue;
			}

			const uint32_t highest_segment_id = (typed_segments.size() - 1);
			if (segment_id > highest_segment_id) {
				output_complete = false;
				continue;
			}

			const auto segment = typed_segments.at(segment_id);
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
		typed_data<T>()->typed_segments().at(segment_id)->set_complete();
}

template <typename T>
void MathSignalBase<T>::reset_generation()
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

template <typename T>
void MathSignalBase<T>::begin_generation()
{
	reset_generation();

	if (expression_.isEmpty()) {
		set_error(MATH_ERR_EMPTY_EXPR, tr("No expression defined, nothing to do"));
		return;
	}

	disconnect(&session_, SIGNAL(data_received()), this, SLOT(on_data_received()));

	for (const shared_ptr<SignalBase>& sb : session_.signalbases()) {
		if (sb->typed_data<T>())
			disconnect(sb->typed_data<T>().get(), nullptr, this, SLOT(on_data_received()));
		disconnect(sb.get(), nullptr, this, SLOT(on_enabled_changed()));
	}

	fnc_sample_ = new fnc_sample<T, double>(*this);

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
			if (!signal || (!signal->typed_data<T>())) {
				set_error(MATH_ERR_INVALID_SIGNAL, QString(tr(T::InvalidSignal)) \
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
			connect(&session_, SIGNAL(data_received()),	this, SLOT(on_data_received()));

		gen_interrupt_ = false;
		gen_thread_ = std::thread(&MathSignalBase<T>::generation_proc, this);
	}
}

template <typename T>
uint64_t MathSignalBase<T>::generate_samples(uint32_t segment_id, const uint64_t start_sample,
	const int64_t sample_count)
{
	uint64_t count = 0;

	auto local_data = dynamic_pointer_cast<T>(data_);
	auto segment = local_data->typed_segments().at(segment_id);

	// Keep the math functions segment IDs in sync
	fnc_sample_->current_segment = segment_id;

	const double sample_rate = data_->get_samplerate();

	exprtk_current_sample_ = start_sample;

	auto *sample_data = new typename T::sample_t[sample_count];

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

template <typename T>
void MathSignalBase<T>::generation_proc()
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
	auto local_typed_data = typed_data<T>();

	// Create initial data segment
	auto output_segment =
		make_shared<typename T::segment_t>(*local_typed_data.get(), segment_id, local_typed_data->get_samplerate());
	local_typed_data->push_segment(output_segment);

	// Create data samples
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
					make_shared<typename T::segment_t>(*local_typed_data.get(), segment_id, local_typed_data->get_samplerate());
				local_typed_data->push_segment(output_segment);
		}

		if (!gen_interrupt_ && (samples_to_process == 0)) {
			// Wait for more input
			unique_lock<mutex> gen_input_lock(input_mutex_);
			gen_input_cond_.wait(gen_input_lock);
		}
	} while (!gen_interrupt_);
}

template <typename T>
signal_data* MathSignalBase<T>::signal_from_name(const std::string& name)
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
				if (!sb->typed_data<T>())
					continue;

				connect(sb->typed_data<T>().get(), SIGNAL(samples_added(SharedPtrToSegment, uint64_t, uint64_t)),
					this, SLOT(on_data_received()));
				connect(sb->typed_data<T>().get(), SIGNAL(segment_completed()),
					this, SLOT(on_data_received()));

				connect(sb.get(), SIGNAL(enabled_changed(bool)),
					this, SLOT(on_enabled_changed()));

				return &(input_signals_.insert({name, signal_data(sb)}).first->second);
			}
	}

	// If we reach this point, no valid signal was found with the supplied name
	if (error_type_ == MATH_ERR_NONE)
		set_error(MATH_ERR_INVALID_SIGNAL, QString(tr(T::InvalidSignal)) \
			.arg(QString::fromStdString(name)));

	return nullptr;
}

template <typename T>
void MathSignalBase<T>::update_signal_sample(signal_data* sig_data, uint32_t segment_id, uint64_t sample_num)
{
	assert(sig_data);

	// Update the value only if a different sample is requested
	if (sig_data->sample_num == sample_num)
		return;

	assert(sig_data->sb);
	const auto local_data = sig_data->sb->typed_data<T>();
	assert(local_data);

	assert(segment_id < local_data->typed_segments().size());

	const auto segment = local_data->typed_segments().at(segment_id);

	sig_data->sample_num = sample_num;

	if (sample_num < segment->get_sample_count())
		sig_data->sample_value = segment->get_sample(sample_num, sig_data->sb->index());
	else
		sig_data->sample_value = 0;

	// We only have a reference if this signal is used as a scalar;
	// if it's used by a function, it's null
	if (sig_data->ref)
		*(sig_data->ref) = sig_data->sample_value;
}

template <typename T>
bool MathSignalBase<T>::all_input_signals_enabled(QString &disabled_signals) const
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

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_MATHSIGNALBASE_HPP
