/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DATA_DECODERSTACK_HPP
#define PULSEVIEW_PV_DATA_DECODERSTACK_HPP

#include "signaldata.hpp"

#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <thread>

#include <boost/optional.hpp>

#include <QObject>
#include <QString>

#include <pv/data/decode/row.hpp>
#include <pv/data/decode/rowdata.hpp>
#include <pv/util.hpp>

struct srd_decoder;
struct srd_decoder_annotation_row;
struct srd_channel;
struct srd_proto_data;
struct srd_session;

namespace DecoderStackTest {
struct TwoDecoderStack;
}

namespace pv {

class Session;

namespace view {
class LogicSignal;
}

namespace data {

class LogicSegment;

namespace decode {
class Annotation;
class Decoder;
}

class Logic;

class DecoderStack : public QObject
{
	Q_OBJECT

private:
	static const double DecodeMargin;
	static const double DecodeThreshold;
	static const int64_t DecodeChunkLength;
	static const unsigned int DecodeNotifyPeriod;

public:
	DecoderStack(pv::Session &session, const srd_decoder *const dec);

	virtual ~DecoderStack();

	const std::list< std::shared_ptr<decode::Decoder> >& stack() const;
	void push(std::shared_ptr<decode::Decoder> decoder);
	void remove(int index);

	double samplerate() const;

	const pv::util::Timestamp& start_time() const;

	int64_t samples_decoded() const;

	std::vector<decode::Row> get_visible_rows() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation> &dest,
		const decode::Row &row, uint64_t start_sample,
		uint64_t end_sample) const;

	QString error_message();

	void clear();

	uint64_t max_sample_count() const;

	void begin_decode();

private:
	boost::optional<int64_t> wait_for_data() const;

	void decode_data(const int64_t abs_start_samplenum, const int64_t sample_count,
		const unsigned int unit_size, srd_session *const session);

	void decode_proc();

	static void annotation_callback(srd_proto_data *pdata,
		void *decoder);

private Q_SLOTS:
	void on_new_frame();

	void on_data_received();

	void on_frame_ended();

Q_SIGNALS:
	void new_decode_data();

private:
	pv::Session &session_;

	pv::util::Timestamp start_time_;
	double samplerate_;

	/**
	 * This mutex prevents more than one thread from accessing
	 * libsigrokdecode concurrently.
	 * @todo A proper solution should be implemented to allow multiple
	 * decode operations in parallel.
	 */
	static std::mutex global_srd_mutex_;

	std::list< std::shared_ptr<decode::Decoder> > stack_;

	std::shared_ptr<pv::data::LogicSegment> segment_;

	mutable std::mutex input_mutex_;
	mutable std::condition_variable input_cond_;
	int64_t sample_count_;
	bool frame_complete_;

	mutable std::mutex output_mutex_;
	int64_t	samples_decoded_;

	std::map<const decode::Row, decode::RowData> rows_;

	std::map<std::pair<const srd_decoder*, int>, decode::Row> class_rows_;

	QString error_message_;

	std::thread decode_thread_;
	std::atomic<bool> interrupt_;

	friend struct DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODERSTACK_HPP
