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
#include <QByteArray>
#include <QFile>

using std::size_t;


class DataStorage
{
public:
	virtual ~DataStorage() {};
	virtual QByteArray getData(size_t position, size_t length) = 0;
	virtual size_t size() = 0;
};


class DataStorageArray: public DataStorage
{
public:
	DataStorageArray(const QByteArray &arr);
	virtual QByteArray getData(size_t position, size_t length);
	virtual size_t size();

private:
	QByteArray data_;
};


class QHexView: public QAbstractScrollArea
{
	QHexView(QWidget *parent = 0);
	~QHexView();

public Q_SLOTS:
	void setData(DataStorage *pData);
	void clear();
	void showFromOffset(size_t offset);

protected:
	void paintEvent(QPaintEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);

private:
	QSize fullSize() const;
	void resetSelection();
	void resetSelection(int pos);
	void setSelection(int pos);
	void ensureVisible();
	void setCursorPos(int pos);
	size_t cursorPos(const QPoint &position);

private:
	DataStorage *pdata_;

	size_t posAddr_, posHex_, posAscii_;
	size_t charWidth_, charHeight_;
	size_t selectBegin_, selectEnd_, selectInit_, cursorPos_;
};

#endif /* PULSEVIEW_PV_VIEWS_DECODEROUTPUT_QHEXVIEW_H */
