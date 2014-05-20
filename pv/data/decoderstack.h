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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_DATA_DECODERSTACK_H
#define PULSEVIEW_PV_DATA_DECODERSTACK_H

#include "signaldata.h"

#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <thread>

#include <boost/optional.hpp>

#include <QObject>
#include <QString>

#include <pv/data/decode/row.h>
#include <pv/data/decode/rowdata.h>

struct srd_decoder;
struct srd_decoder_annotation_row;
struct srd_channel;
struct srd_proto_data;
struct srd_session;

namespace DecoderStackTest {
class TwoDecoderStack;
}

namespace pv {

class SigSession;

namespace view {
class LogicSignal;
}

namespace data {

class LogicSnapshot;

namespace decode {
class Annotation;
class Decoder;
}

class Logic;

class DecoderStack : public QObject, public SignalData
{
	Q_OBJECT

private:
	static const double DecodeMargin;
	static const double DecodeThreshold;
	static const int64_t DecodeChunkLength;
	static const unsigned int DecodeNotifyPeriod;

public:
	DecoderStack(pv::SigSession &_session,
		const srd_decoder *const decoder);

	virtual ~DecoderStack();

	const std::list< std::shared_ptr<decode::Decoder> >& stack() const;
	void push(std::shared_ptr<decode::Decoder> decoder);
	void remove(int index);

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

	uint64_t get_max_sample_count() const;

	void begin_decode();

private:
	boost::optional<int64_t> wait_for_data() const;

	void decode_data(const int64_t sample_count,
		const unsigned int unit_size, srd_session *const session);

	void decode_proc();

	static void annotation_callback(srd_proto_data *pdata,
		void *decoder);

private slots:
	void on_new_frame();

	void on_data_received();

	void on_frame_ended();

signals:
	void new_decode_data();

private:
	pv::SigSession &_session;

	/**
	 * This mutex prevents more than one decode operation occuring
	 * concurrently.
	 * @todo A proper solution should be implemented to allow multiple
	 * decode operations.
	 */
	static std::mutex _global_decode_mutex;

	std::list< std::shared_ptr<decode::Decoder> > _stack;

	std::shared_ptr<pv::data::LogicSnapshot> _snapshot;

	mutable std::mutex _input_mutex;
	mutable std::condition_variable _input_cond;
	int64_t _sample_count;
	bool _frame_complete;

	mutable std::mutex _output_mutex;
	int64_t	_samples_decoded;

	std::map<const decode::Row, decode::RowData> _rows;

	std::map<std::pair<const srd_decoder*, int>, decode::Row> _class_rows;

	QString _error_message;

	std::thread _decode_thread;
	std::atomic<bool> _interrupt;

	friend class DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODERSTACK_H
