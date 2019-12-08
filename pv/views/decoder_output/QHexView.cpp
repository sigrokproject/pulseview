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

const unsigned int BYTES_PER_LINE   = 16;
const unsigned int HEXCHARS_IN_LINE = BYTES_PER_LINE * 3 - 1;
const unsigned int GAP_ADR_HEX      = 10;
const unsigned int GAP_HEX_ASCII    = 10;
const unsigned int GAP_ASCII_SLIDER = 5;


QHexView::QHexView(QWidget *parent):
	QAbstractScrollArea(parent),
	data_(nullptr)
{
	setFont(QFont("Courier", 10));

	charWidth_ = fontMetrics().boundingRect('X').width();
	charHeight_ = fontMetrics().height();

	// Determine X coordinates of the three sub-areas
	posAddr_  = 0;
	posHex_   = 10 * charWidth_ + GAP_ADR_HEX;
	posAscii_ = posHex_ + HEXCHARS_IN_LINE * charWidth_ + GAP_HEX_ASCII;

	setFocusPolicy(Qt::StrongFocus);
}

void QHexView::setData(QByteArray *data)
{
	verticalScrollBar()->setValue(0);

	data_ = data;
	cursorPos_ = 0;
	resetSelection(0);
}

void QHexView::showFromOffset(size_t offset)
{
	if (data_ && (offset < (size_t)data_->count())) {
		setCursorPos(offset * 2);

		int cursorY = cursorPos_ / (2 * BYTES_PER_LINE);
		verticalScrollBar() -> setValue(cursorY);
	}

	viewport()->update();
}

void QHexView::clear()
{
	verticalScrollBar()->setValue(0);
	data_ = nullptr;

	viewport()->update();
}

QSize QHexView::getFullSize() const
{
	size_t width = posAscii_ + (BYTES_PER_LINE * charWidth_) +
		GAP_ASCII_SLIDER + verticalScrollBar()->width();

	if (!data_)
		return QSize(width, 0);

	size_t height = data_->count() / BYTES_PER_LINE;

	if (data_->count() % BYTES_PER_LINE)
		height++;

	height *= charHeight_;

	return QSize(width, height);
}

void QHexView::paintEvent(QPaintEvent *event)
{
	QPainter painter(viewport());

	// Calculate and update the widget and paint area sizes
	QSize widgetSize = getFullSize();
	setMinimumWidth(widgetSize.width());
	setMaximumWidth(widgetSize.width());
	QSize areaSize = viewport()->size();

	verticalScrollBar()->setPageStep(areaSize.height() / charHeight_);
	verticalScrollBar()->setRange(0, (widgetSize.height() - areaSize.height()) / charHeight_ + 1);

	// Fill widget background
	painter.fillRect(event->rect(), palette().color(QPalette::Base));

	if (!data_ || (data_->size() == 0)) {
		painter.setPen(palette().color(QPalette::Text));
		QString s = tr("No data available");
		int x = (areaSize.width() - fontMetrics().boundingRect(s).width()) / 2;
		int y = areaSize.height() / 2;
		painter.drawText(x, y, s);
		return;
	}

	// Determine first/last line indices
	size_t firstLineIdx = verticalScrollBar()->value();

	size_t lastLineIdx = firstLineIdx + areaSize.height() / charHeight_;
	if (lastLineIdx > (data_->count() / BYTES_PER_LINE)) {
		lastLineIdx = data_->count() / BYTES_PER_LINE;
		if (data_->count() % BYTES_PER_LINE)
			lastLineIdx++;
	}

	// Fill address area background
	painter.fillRect(QRect(posAddr_, event->rect().top(),
		posHex_ - (GAP_ADR_HEX / 2), height()), palette().color(QPalette::Window));

	// Paint divider line between hex and ASCII areas
	int line_x = posAscii_ - (GAP_HEX_ASCII / 2);
	painter.setPen(palette().color(QPalette::Midlight));
	painter.drawLine(line_x, event->rect().top(), line_x, height());

	// Paint address area
	painter.setPen(palette().color(QPalette::ButtonText));

	int yStart = charHeight_;
	for (size_t lineIdx = firstLineIdx, y = yStart; lineIdx < lastLineIdx; lineIdx++) {

		QString address = QString("%1").arg(lineIdx * 16, 10, 16, QChar('0')).toUpper();
		painter.drawText(posAddr_, y, address);
		y += charHeight_;
	}

	// Paint hex values
	QBrush regular = painter.brush();
	QBrush selected = QBrush(palette().color(QPalette::Highlight));
	QByteArray data = data_->mid(firstLineIdx * BYTES_PER_LINE,
		(lastLineIdx - firstLineIdx) * BYTES_PER_LINE);

	yStart = charHeight_;
	for (size_t lineIdx = firstLineIdx, y = yStart; lineIdx < lastLineIdx; lineIdx++) {

		painter.setBackgroundMode(Qt::OpaqueMode);

		int x = posHex_;
		for (size_t i = 0; i < BYTES_PER_LINE && ((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) < (size_t)data.size(); i++) {
			size_t pos = (lineIdx * BYTES_PER_LINE + i) * 2;

			if ((pos >= selectBegin_) && (pos < selectEnd_)) {
				painter.setBackground(selected);
				painter.setPen(palette().color(QPalette::HighlightedText));
			} else {
				painter.setBackground(regular);
				painter.setPen(palette().color(QPalette::Text));
			}

			// First nibble
			QString val = QString::number((data.at((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) & 0xF0) >> 4, 16).toUpper();
			painter.drawText(x, y, val);

			// Second nibble
			val = QString::number((data.at((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) & 0xF), 16).toUpper();
			painter.drawText(x + charWidth_, y, val);

			x += 3 * charWidth_;
		}

		y += charHeight_;
	}

	// Paint ASCII characters
	yStart = charHeight_;
	for (size_t lineIdx = firstLineIdx, y = yStart; lineIdx < lastLineIdx; lineIdx++) {

		int x = posAscii_;
		for (size_t i = 0; ((lineIdx - firstLineIdx) * BYTES_PER_LINE + i) < (size_t)data.size() && (i < BYTES_PER_LINE); i++) {
			char ch = data[(unsigned int)((lineIdx - firstLineIdx) * BYTES_PER_LINE + i)];

			if ((ch < 0x20) || (ch > 0x7E))
				ch = '.';

			size_t pos = (lineIdx * BYTES_PER_LINE + i) * 2;
			if ((pos >= selectBegin_) && (pos < selectEnd_)) {
				painter.setBackground(selected);
				painter.setPen(palette().color(QPalette::HighlightedText));
			} else {
				painter.setBackground(regular);
				painter.setPen(palette().color(QPalette::Text));
			}

			painter.drawText(x, y, QString(ch));
			x += charWidth_;
		}

		y += charHeight_;
	}

	// Paint cursor
	if (hasFocus()) {
		int x = (cursorPos_ % (2 * BYTES_PER_LINE));
		int y = cursorPos_ / (2 * BYTES_PER_LINE);
		y -= firstLineIdx;
		int cursorX = (((x / 2) * 3) + (x % 2)) * charWidth_ + posHex_;
		int cursorY = y * charHeight_ + 4;
		painter.fillRect(cursorX, cursorY, 2, charHeight_, palette().color(QPalette::WindowText));
	}
}

void QHexView::keyPressEvent(QKeyEvent *event)
{
	bool setVisible = false;

	// Cursor movements
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
		if (data_)
			setCursorPos(data_->count() * 2);
		resetSelection(cursorPos_);
		setVisible = true;
	}
	if (event->matches(QKeySequence::MoveToStartOfDocument)) {
		setCursorPos(0);
		resetSelection(cursorPos_);
		setVisible = true;
	}

	// Select commands
	if (event->matches(QKeySequence::SelectAll)) {
		resetSelection(0);
		if (data_)
			setSelection(2 * data_->count() + 1);
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
		if (data_)
			pos = data_->count() * 2;
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

	if (event->matches(QKeySequence::Copy) && (data_)) {
		QString text;
		int idx = 0;
		int copyOffset = 0;

		QByteArray data = data_->mid(selectBegin_ / 2, (selectEnd_ - selectBegin_) / 2 + 1);
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
	int actPos = cursorPosFromMousePos(event->pos());
	setCursorPos(actPos);
	setSelection(actPos);

	viewport()->update();
}

void QHexView::mousePressEvent(QMouseEvent *event)
{
	int cPos = cursorPosFromMousePos(event->pos());

	if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) && (event->button() == Qt::LeftButton))
		setSelection(cPos);
	else
		resetSelection(cPos);

	setCursorPos(cPos);

	viewport()->update();
}

size_t QHexView::cursorPosFromMousePos(const QPoint &position)
{
	int pos = -1;

	if (((size_t)position.x() >= posHex_) &&
		((size_t)position.x() < (posHex_ + HEXCHARS_IN_LINE * charWidth_))) {

		// Note: We add 1.5 character widths so that selection across
		// byte gaps is smoother
		int x = (position.x() + (1.5 * charWidth_ / 2) - posHex_) / charWidth_;

		// Note: We allow only full bytes to be selected, not nibbles,
		// so we round to the nearest byte gap
		x = (2 * x + 1) / 3;

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
	if (data_) {
		maxPos = data_->count() * 2;
		if (data_->count() % BYTES_PER_LINE)
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
