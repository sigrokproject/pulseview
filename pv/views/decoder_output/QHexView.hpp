/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Victor Anjin <virinext@gmail.com>
 * Copyright (C) 2019 Soeren Apel <soeren@apelpie.net>
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PULSEVIEW_PV_VIEWS_DECODEROUTPUT_QHEXVIEW_H
#define PULSEVIEW_PV_VIEWS_DECODEROUTPUT_QHEXVIEW_H

#include <QAbstractScrollArea>

#include <pv/data/decodesignal.hpp>

using std::size_t;
using pv::data::DecodeBinaryClass;
using pv::data::DecodeBinaryDataChunk;

class QHexView: public QAbstractScrollArea
{
	Q_OBJECT

public:
	enum Mode {
		ChunkedDataMode,    ///< Displays all data chunks in succession
		MemoryEmulationMode ///< Reconstructs memory contents from data chunks
	};

public:
	QHexView(QWidget *parent = 0);

	void setMode(Mode m);
	void setData(const DecodeBinaryClass* data);

	void clear();
	void showFromOffset(size_t offset);
	virtual QSizePolicy sizePolicy() const;

protected:
	void initialize_byte_iterator(size_t offset);
	uint8_t get_next_byte(bool* is_next_chunk = nullptr);

	void paintEvent(QPaintEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);

private:
	QSize getFullSize() const;
	void resetSelection();
	void resetSelection(int pos);
	void setSelection(int pos);
	void ensureVisible();
	void setCursorPos(int pos);
	size_t cursorPosFromMousePos(const QPoint &position);

private:
	Mode mode_;
	const DecodeBinaryClass* data_;
	size_t data_size_;

	size_t posAddr_, posHex_, posAscii_;
	size_t charWidth_, charHeight_;
	size_t selectBegin_, selectEnd_, selectInit_, cursorPos_;

	size_t current_chunk_id_, current_chunk_offset_, current_offset_;
	DecodeBinaryDataChunk current_chunk_; // Cache locally so that we're not messed up when the vector is re-allocating its data

	vector<QColor> chunk_colors_;
};

#endif /* PULSEVIEW_PV_VIEWS_DECODEROUTPUT_QHEXVIEW_H */
