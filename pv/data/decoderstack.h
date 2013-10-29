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

namespace decode {
class Annotation;
}

}

namespace data {

namespace decode {
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

	const std::vector< boost::shared_ptr<pv::view::decode::Annotation> >
		annotations() const;

	QString error_message();

	void clear_snapshots();

	void begin_decode();

private:
	void decode_proc(boost::shared_ptr<data::Logic> data);

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
	std::vector< boost::shared_ptr<pv::view::decode::Annotation> >
		_annotations;
	QString _error_message;

	boost::thread _decode_thread;

	friend class DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODERSTACK_H
