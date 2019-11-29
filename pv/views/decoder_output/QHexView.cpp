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

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSize>
#include <QPainter>
#include <QPaintEvent>

#include "QHexView.hpp"

using std::size_t;

const unsigned int HEXCHARS_IN_LINE = 47;
const unsigned int GAP_ADR_HEX = 10;
const unsigned int GAP_HEX_ASCII = 16;
const unsigned int BYTES_PER_LINE = 16;


DataStorageArray::DataStorageArray(const QByteArray &arr)
{
	data_ = arr;
}

QByteArray DataStorageArray::getData(size_t position, size_t length)
{
	return data_.mid(position, length);
}

size_t DataStorageArray::size()
{
	return data_.count();
}



QHexView::QHexView(QWidget *parent):
		QAbstractScrollArea(parent),
		pdata_(nullptr)
{
	setFont(QFont("Courier", 10));

	charWidth_ = fontMetrics().width(QLatin1Char('9'));
	charHeight_ = fontMetrics().height();

	posAddr_ = 0;
	posHex_ = 10 * charWidth_ + GAP_ADR_HEX;
	posAscii_ = posHex_ + HEXCHARS_IN_LINE * charWidth_ + GAP_HEX_ASCII;

	setMinimumWidth(posAscii_ + (BYTES_PER_LINE * charWidth_));

	setFocusPolicy(Qt::StrongFocus);
}

QHexView::~QHexView()
{
	if (pdata_)
		delete pdata_;
}

void QHexView::setData(DataStorage *pData)
{
	verticalScrollBar()->setValue(0);

	if (pdata_)
		delete pdata_;

	pdata_ = pData;
	cursorPos_ = 0;
	resetSelection(0);
}

void QHexView::showFromOffset(size_t offset)
{
	if (pdata_ && (offset < pdata_->size())) {
		setCursorPos(offset * 2);

		int cursorY = cursorPos_ / (2 * BYTES_PER_LINE);
		verticalScrollBar() -> setValue(cursorY);
	}
}

void QHexView::clear()
{
	verticalScrollBar()->setValue(0);
}

QSize QHexView::fullSize() const
{
	if (!pdata_)
		return QSize(0, 0);

	size_t width = posAscii_ + (BYTES_PER_LINE * charWidth_);
	size_t height = pdata_->size() / BYTES_PER_LINE;
	if (pdata_->size() % BYTES_PER_LINE)
		height++;

	height *= charHeight_;

	return QSize(width, height);
}

void QHexView::paintEvent(QPaintEvent *event)
{
	if (!pdata_)
		return;

	QPainter painter(viewport());

	QSize areaSize = viewport()->size();
	QSize widgetSize = fullSize();
	verticalScrollBar()->setPageStep(areaSize.height() / charHeight_);
	verticalScrollBar()->setRange(0, (widgetSize.height() - areaSize.height()) / charHeight_ + 1);

	size_t firstLineIdx = verticalScrollBar()->value();

	size_t lastLineIdx = firstLineIdx + areaSize.height() / charHeight_;
	if (lastLineIdx > (pdata_->size() / BYTES_PER_LINE)) {
		lastLineIdx = pdata_->size() / BYTES_PER_LINE;
		if (pdata_->size() % BYTES_PER_LINE)
			lastLineIdx++;
	}

	painter.fillRect(event->rect(), this->palette().color(QPalette::Base));

	QColor addressAreaColor = QColor(0xd4, 0xd4, 0xd4, 0xff);
	painter.fillRect(QRect(posAddr_, event->rect().top(),
		posHex_ - GAP_ADR_HEX + 2, height()), addressAreaColor);

	int linePos = posAscii_ - (GAP_HEX_ASCII / 2);
	painter.setPen(Qt::gray);

	painter.drawLine(linePos, event->rect().top(), linePos, height());

	painter.setPen(Qt::black);

	int yPosStart = charHeight_;

	QBrush def = painter.brush();
	QBrush selected = QBrush(QColor(0x6d, 0x9e, 0xff, 0xff));
	QByteArray data = pdata_->getData(firstLineIdx * BYTES_PER_LINE, (lastLineIdx - firstLineIdx) * BYTES_PER_LINE);

	for (size_t lineIdx = firstLineIdx, yPos = yPosStart; lineIdx < lastLineIdx; lineIdx++) {
		QString address = QString("%1").arg(lineIdx * 16, 10, 16, QChar('0'));
		painter.drawText(posAddr_, yPos, address);

		int xPos = posHex_;
		for (size_t i = 0; i < BYTES_PER_LINE && ((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) < (size_t)data.size(); i++) {
			size_t pos = (lineIdx * BYTES_PER_LINE + i) * 2;
			if ((pos >= selectBegin_) && (pos < selectEnd_)) {
				painter.setBackground(selected);
				painter.setBackgroundMode(Qt::OpaqueMode);
			}

			QString val = QString::number((data.at((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) & 0xF0) >> 4, 16);
			painter.drawText(xPos, yPos, val);

			if (((pos+1) >= selectBegin_) && ((pos+1) < selectEnd_)) {
				painter.setBackground(selected);
				painter.setBackgroundMode(Qt::OpaqueMode);
			} else {
				painter.setBackground(def);
				painter.setBackgroundMode(Qt::OpaqueMode);
			}

			val = QString::number((data.at((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) & 0xF), 16);
			painter.drawText(xPos + charWidth_, yPos, val);

			painter.setBackground(def);
			painter.setBackgroundMode(Qt::OpaqueMode);

			xPos += 3 * charWidth_;
		}

		int xPosAscii = posAscii_;
		for (size_t i = 0; ((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) < (size_t)data.size() && (i < BYTES_PER_LINE); i++) {
			char ch = data[(unsigned int)((lineIdx - firstLineIdx) * BYTES_PER_LINE + i)];

			if ((ch < 0x20) || (ch > 0x7e))
				ch = '.';

			painter.drawText(xPosAscii, yPos, QString(ch));
			xPosAscii += charWidth_;
		}

		yPos += charHeight_;
	}

	if (hasFocus()) {
		int x = (cursorPos_ % (2 * BYTES_PER_LINE));
		int y = cursorPos_ / (2 * BYTES_PER_LINE);
		y -= firstLineIdx;
		int cursorX = (((x / 2) * 3) + (x % 2)) * charWidth_ + posHex_;
		int cursorY = y * charHeight_ + 4;
		painter.fillRect(cursorX, cursorY, 2, charHeight_, this->palette().color(QPalette::WindowText));
	}
}

void QHexView::keyPressEvent(QKeyEvent *event)
{
	bool setVisible = false;

	/*****************************************************************************/
	/* Cursor movements */
	/*****************************************************************************/
	if (event->matches(QKeySequence::MoveToNextChar)) {
		setCursorPos(cursorPos_ + 1);
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToPreviousChar)) {
		setCursorPos(cursorPos_ - 1);
		resetSelection(cursorPos_);
		setVisible = true;
	}

	if (event->matches(QKeySequence::MoveToEndOfLine)) {
		setCursorPos(cursorPos_ | ((BYTES_PER_LINE * 2) - 1));
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToStartOfLine)) {
		setCursorPos(cursorPos_ | (cursorPos_ % (BYTES_PER_LINE * 2)));
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToPreviousLine)) {
		setCursorPos(cursorPos_ - BYTES_PER_LINE * 2);
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToNextLine)) {
		setCursorPos(cursorPos_ + BYTES_PER_LINE * 2);
		resetSelection(cursorPos_);
		setVisible = true;
	}

	if (event->matches(QKeySequence::MoveToNextPage)) {
		setCursorPos(cursorPos_ + (viewport()->height() / charHeight_ - 1) * 2 * BYTES_PER_LINE);
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToPreviousPage)) {
		setCursorPos(cursorPos_ - (viewport()->height() / charHeight_ - 1) * 2 * BYTES_PER_LINE);
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToEndOfDocument)) {
		if (pdata_)
			setCursorPos(pdata_->size() * 2);
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToStartOfDocument)) {
		setCursorPos(0);
		resetSelection(cursorPos_);
		setVisible = true;
	}

	/*****************************************************************************/
	/* Select commands */
	/*****************************************************************************/
	if (event->matches(QKeySequence::SelectAll)) {
		resetSelection(0);
		if (pdata_)
			setSelection(2 * pdata_->size() + 1);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectNextChar)) {
		int pos = cursorPos_ + 1;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousChar)) {
		int pos = cursorPos_ - 1;
		setSelection(pos);
		setCursorPos(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectEndOfLine)) {
		int pos = cursorPos_ - (cursorPos_ % (2 * BYTES_PER_LINE)) + (2 * BYTES_PER_LINE);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfLine)) {
		int pos = cursorPos_ - (cursorPos_ % (2 * BYTES_PER_LINE));
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousLine)) {
		int pos = cursorPos_ - (2 * BYTES_PER_LINE);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectNextLine)) {
		int pos = cursorPos_ + (2 * BYTES_PER_LINE);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}

	if (event->matches(QKeySequence::SelectNextPage)) {
		int pos = cursorPos_ + (((viewport()->height() / charHeight_) - 1) * 2 * BYTES_PER_LINE);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectPreviousPage)) {
		int pos = cursorPos_ - (((viewport()->height() / charHeight_) - 1) * 2 * BYTES_PER_LINE);
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectEndOfDocument)) {
		int pos = 0;
		if (pdata_)
			pos = pdata_->size() * 2;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfDocument)) {
		int pos = 0;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}

	if (event->matches(QKeySequence::Copy) && (pdata_)) {
		QString text;
		int idx = 0;
		int copyOffset = 0;

		QByteArray data = pdata_->getData(selectBegin_ / 2, (selectEnd_ - selectBegin_) / 2 + 1);
		if (selectBegin_ % 2) {
			text += QString::number((data.at((idx+1) / 2) & 0xF), 16);
			text += " ";
			idx++;
			copyOffset = 1;
		}

		int selectedSize = selectEnd_ - selectBegin_;
		while (idx < selectedSize) {
			QString val = QString::number((data.at((copyOffset + idx) / 2) & 0xF0) >> 4, 16);
			if ((idx + 1) < selectedSize) {
				val += QString::number((data.at((copyOffset + idx) / 2) & 0xF), 16);
				val += " ";
			}
			text += val;

			if ((idx/2) % BYTES_PER_LINE == (BYTES_PER_LINE - 1))
				text += "\n";

			idx += 2;
		}

		QClipboard *clipboard = QApplication::clipboard();
		clipboard->setText(text);
		if (clipboard->supportsSelection())
			clipboard->setText(text);
	}

	if (setVisible)
		ensureVisible();

	viewport()->update();
}

void QHexView::mouseMoveEvent(QMouseEvent *event)
{
	int actPos = cursorPos(event->pos());
	setCursorPos(actPos);
	setSelection(actPos);

	viewport()->update();
}

void QHexView::mousePressEvent(QMouseEvent *event)
{
	int cPos = cursorPos(event->pos());

	if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) && (event->button() == Qt::LeftButton))
		setSelection(cPos);
	else
		resetSelection(cPos);

	setCursorPos(cPos);

	viewport()->update();
}

size_t QHexView::cursorPos(const QPoint &position)
{
	int pos = -1;

	if (((size_t)position.x() >= posHex_) &&
		((size_t)position.x() < (posHex_ + HEXCHARS_IN_LINE * charWidth_))) {

		int x = (position.x() - posHex_) / charWidth_;

		if ((x % 3) == 0)
			x = (x / 3) * 2;
		else
			x = ((x / 3) * 2) + 1;

		int firstLineIdx = verticalScrollBar()->value();
		int y = (position.y() / charHeight_) * 2 * BYTES_PER_LINE;
		pos = x + y + firstLineIdx * BYTES_PER_LINE * 2;
	}

	return pos;
}

void QHexView::resetSelection()
{
	selectBegin_ = selectInit_;
	selectEnd_ = selectInit_;
}

void QHexView::resetSelection(int pos)
{
	if (pos < 0)
		pos = 0;

	selectInit_ = pos;
	selectBegin_ = pos;
	selectEnd_ = pos;
}

void QHexView::setSelection(int pos)
{
	if (pos < 0)
		pos = 0;

	if ((size_t)pos >= selectInit_) {
		selectEnd_ = pos;
		selectBegin_ = selectInit_;
	} else {
		selectBegin_ = pos;
		selectEnd_ = selectInit_;
	}
}

void QHexView::setCursorPos(int position)
{
	if (position < 0)
		position = 0;

	int maxPos = 0;
	if (pdata_) {
		maxPos = pdata_->size() * 2;
		if (pdata_->size() % BYTES_PER_LINE)
			maxPos++;
	}

	if (position > maxPos)
		position = maxPos;

	cursorPos_ = position;
}

void QHexView::ensureVisible()
{
	QSize areaSize = viewport()->size();

	int firstLineIdx = verticalScrollBar() -> value();
	int lastLineIdx = firstLineIdx + areaSize.height() / charHeight_;

	int cursorY = cursorPos_ / (2 * BYTES_PER_LINE);

	if (cursorY < firstLineIdx)
		verticalScrollBar() -> setValue(cursorY);
	else if(cursorY >= lastLineIdx)
		verticalScrollBar() -> setValue(cursorY - areaSize.height() / charHeight_ + 1);
}
