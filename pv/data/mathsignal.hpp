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

#ifndef PULSEVIEW_PV_DATA_MATHSIGNAL_HPP
#define PULSEVIEW_PV_DATA_MATHSIGNAL_HPP

#define exprtk_disable_rtl_io_file /* Potential security issue, doubt anyone would use those anyway */
#define exprtk_disable_rtl_vecops  /* Vector ops are rather useless for math channels */
#define exprtk_disable_caseinsensitivity /* So that we can have both 't' and 'T' */

#include <limits>

#include <QString>

#include <pv/exprtk.hpp>
#include <pv/util.hpp>
#include <pv/data/analog.hpp>
#include <pv/data/signalbase.hpp>

using std::atomic;
using std::condition_variable;
using std::mutex;
using std::numeric_limits;
using std::shared_ptr;

namespace pv {
class Session;

namespace data {

class SignalBase;

template<typename T>
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

class MathSignal : public SignalBase
{
	Q_OBJECT
	Q_PROPERTY(QString expression READ get_expression WRITE set_expression NOTIFY expression_changed)

private:
	static const int64_t ChunkLength;

public:
	MathSignal(pv::Session &session);
	virtual ~MathSignal();

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

	QString get_expression() const;
	void set_expression(QString expression);

private:
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

Q_SIGNALS:
	void expression_changed(QString expression);

private Q_SLOTS:
	void on_capture_state_changed(int state);
	void on_data_received();

	void on_enabled_changed();

private:
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

	fnc_sample<double>* fnc_sample_;

	// Give sig_sample access to the private helper functions
	friend struct fnc_sample<double>;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_MATHSIGNAL_HPP
