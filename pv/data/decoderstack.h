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

#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <QObject>
#include <QString>

struct srd_decoder;
struct srd_probe;
struct srd_proto_data;

namespace DecoderStackTest {
class TwoDecoderStack;
}

namespace pv {

namespace view {
class LogicSignal;
}

namespace data {

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

public:
	DecoderStack(const srd_decoder *const decoder);

	virtual ~DecoderStack();

	const std::list< boost::shared_ptr<decode::Decoder> >& stack() const;
	void push(boost::shared_ptr<decode::Decoder> decoder);
	void remove(int index);

	int64_t samples_decoded() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation> &dest,
		uint64_t start_sample, uint64_t end_sample) const;

	QString error_message();

	void clear();

	uint64_t get_max_sample_count() const;

	void begin_decode();

private:
	void decode_proc(boost::shared_ptr<data::Logic> data);

	bool index_entry_start_sample_gt(
		const uint64_t sample, const size_t index) const;
	bool index_entry_end_sample_lt(
		const size_t index, const uint64_t sample) const;
	bool index_entry_end_sample_gt(
		const uint64_t sample, const size_t index) const;

	void insert_annotation_into_start_index(
		const pv::data::decode::Annotation &a,
		const size_t storage_offset);
	void insert_annotation_into_end_index(
		const pv::data::decode::Annotation &a,
		const size_t storage_offset);

	static void annotation_callback(srd_proto_data *pdata,
		void *decoder);

signals:
	void new_decode_data();

private:

	/**
	 * This mutex prevents more than one decode operation occuring
	 * concurrently.
	 * @todo A proper solution should be implemented to allow multiple
	 * decode operations.
	 */
	static boost::mutex _global_decode_mutex;

	std::list< boost::shared_ptr<decode::Decoder> > _stack;

	mutable boost::mutex _mutex;
	int64_t	_samples_decoded;
	std::vector<pv::data::decode::Annotation> _annotations;

	/**
	 * _ann_start_index and _ann_end_index contain lists of annotions
	 * (represented by offsets in the _annotations vector), sorted in
	 * ascending ordered by the start_sample and end_sample respectively.
	 */
	std::vector<size_t> _ann_start_index, _ann_end_index;

	QString _error_message;

	boost::thread _decode_thread;

	friend class DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODERSTACK_H
