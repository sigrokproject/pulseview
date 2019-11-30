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

class QHexView: public QAbstractScrollArea
{
public:
	QHexView(QWidget *parent = 0);

	void setData(QByteArray *data);

public Q_SLOTS:
	void clear();
	void showFromOffset(size_t offset);

protected:
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
	QByteArray *data_;

	size_t posAddr_, posHex_, posAscii_;
	size_t charWidth_, charHeight_;
	size_t selectBegin_, selectEnd_, selectInit_, cursorPos_;
};

#endif /* PULSEVIEW_PV_VIEWS_DECODEROUTPUT_QHEXVIEW_H */
