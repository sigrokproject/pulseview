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

#include <limits>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFont>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QPainter>
#include <QPaintEvent>

#include "QHexView.hpp"

using std::make_pair;

const unsigned int BYTES_PER_LINE   = 16;
const unsigned int HEXCHARS_IN_LINE = BYTES_PER_LINE * 3 - 1;
const unsigned int GAP_ADR_HEX      = 10;
const unsigned int GAP_HEX_ASCII    = 10;
const unsigned int GAP_ASCII_SLIDER = 5;


QHexView::QHexView(QWidget *parent):
	QAbstractScrollArea(parent),
	mode_(ChunkedDataMode),
	data_(nullptr),
	selectBegin_(0),
	selectEnd_(0),
	cursorPos_(0),
	visible_range_(0, 0),
	highlighted_sample_(std::numeric_limits<uint64_t>::max())
{
	setFont(QFont("Courier", 10));

	charWidth_ = fontMetrics().boundingRect('X').width();
	charHeight_ = fontMetrics().height();

	setFocusPolicy(Qt::StrongFocus);

	if (palette().color(QPalette::ButtonText).toHsv().value() > 127) {
		// Color is bright
		chunk_colors_.emplace_back(100, 149, 237); // QColorConstants::Svg::cornflowerblue
		chunk_colors_.emplace_back(60, 179, 113);  // QColorConstants::Svg::mediumseagreen
		chunk_colors_.emplace_back(210, 180, 140); // QColorConstants::Svg::tan
		visible_range_color_ = QColor("#fff5ee");  // QColorConstants::Svg::seashell
	} else {
		// Color is dark
		chunk_colors_.emplace_back(0, 0, 139);    // QColorConstants::Svg::darkblue
		chunk_colors_.emplace_back(34, 139, 34);  // QColorConstants::Svg::forestgreen
		chunk_colors_.emplace_back(160, 82, 45);  // QColorConstants::Svg::sienna
		visible_range_color_ = QColor("#fff5ee"); // QColorConstants::Svg::seashell
	}
}

void QHexView::set_mode(Mode m)
{
	mode_ = m;

	// This is not expected to be set when data is showing,
	// so we don't update the viewport here
}

void QHexView::set_data(const DecodeBinaryClass* data)
{
	data_ = data;

	size_t size = 0;
	if (data) {
		size_t chunks = data_->chunks.size();
		for (size_t i = 0; i < chunks; i++)
			size += data_->chunks[i].data.size();
	}
	data_size_ = size;

	address_digits_ = (uint8_t)QString::number(data_size_, 16).length();

	// Calculate X coordinates of the three sub-areas
	posAddr_  = 0;
	posHex_   = address_digits_ * charWidth_ + GAP_ADR_HEX;
	posAscii_ = posHex_ + HEXCHARS_IN_LINE * charWidth_ + GAP_HEX_ASCII;

	viewport()->update();
}

void QHexView::set_visible_sample_range(uint64_t start, uint64_t end)
{
	visible_range_ = make_pair(start, end);

	viewport()->update();
}

void QHexView::set_highlighted_data_sample(uint64_t sample)
{
	highlighted_sample_ = sample;

	viewport()->update();
}

unsigned int QHexView::get_bytes_per_line() const
{
	return BYTES_PER_LINE;
}

void QHexView::clear()
{
	verticalScrollBar()->setValue(0);
	data_ = nullptr;
	data_size_ = 0;

	highlighted_sample_ = std::numeric_limits<uint64_t>::max();

	viewport()->update();
}

void QHexView::showFromOffset(size_t offset)
{
	if (data_ && (offset < data_size_)) {
		setCursorPos(offset * 2);

		int cursorY = cursorPos_ / (2 * BYTES_PER_LINE);
		verticalScrollBar() -> setValue(cursorY);
	}

	viewport()->update();
}

QSizePolicy QHexView::sizePolicy() const
{
	return QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

pair<size_t, size_t> QHexView::get_selection() const
{
	size_t start = selectBegin_ / 2;
	size_t end = selectEnd_ / 2;

	if (start == end) {
		// Nothing is currently selected
		start = 0;
		end = data_size_;
	} if (end < data_size_)
		end++;

	return std::make_pair(start, end);
}

size_t QHexView::create_hex_line(size_t start, size_t end, QString* dest,
	bool with_offset, bool with_ascii)
{
	dest->clear();

	// Determine start address for the row
	uint64_t row = start / BYTES_PER_LINE;
	uint64_t offset = row * BYTES_PER_LINE;
	end = std::min((uint64_t)end, offset + BYTES_PER_LINE);

	if (with_offset)
		dest->append(QString("%1 ").arg(row * BYTES_PER_LINE, address_digits_, 16, QChar('0')).toUpper());

	initialize_byte_iterator(offset);
	for (size_t i = offset; i < offset + BYTES_PER_LINE; i++) {
		uint8_t value = 0;

		if (i < end)
			value = get_next_byte();

		if ((i < start) || (i >= end))
			dest->append("   ");
		else
			dest->append(QString("%1 ").arg(value, 2, 16, QChar('0')).toUpper());
	}

	if (with_ascii) {
		initialize_byte_iterator(offset);
		for (size_t i = offset; i < end; i++) {
			uint8_t value = get_next_byte();

			if ((value < 0x20) || (value > 0x7E))
				value = '.';

			if (i < start)
				dest->append(' ');
			else
				dest->append((char)value);
		}
	}

	return end;
}

void QHexView::initialize_byte_iterator(size_t offset)
{
	current_chunk_id_ = 0;
	current_chunk_offset_ = 0;
	current_offset_ = offset;

	size_t chunks = data_->chunks.size();
	for (size_t i = 0; i < chunks; i++) {
		size_t size = data_->chunks[i].data.size();

		if (offset >= size) {
			current_chunk_id_++;
			offset -= size;
		} else {
			current_chunk_offset_ = offset;
			break;
		}
	}

	if (current_chunk_id_ < data_->chunks.size())
		current_chunk_ = data_->chunks[current_chunk_id_];

	current_chunk_sample_ = current_chunk_.sample;

	// Obtain sample of next chunk if there is one
	if ((current_chunk_id_ + 1) < data_->chunks.size())
		next_chunk_sample_ = data_->chunks[current_chunk_id_ + 1].sample;
	else
		next_chunk_sample_ = std::numeric_limits<uint64_t>::max();
}

uint8_t QHexView::get_next_byte(bool* is_new_chunk)
{
	if (is_new_chunk != nullptr)
		*is_new_chunk = (current_chunk_offset_ == 0);

	uint8_t v = 0;
	if (current_chunk_offset_ < current_chunk_.data.size())
		v = current_chunk_.data[current_chunk_offset_];

	current_chunk_sample_ = current_chunk_.sample;

	if (is_new_chunk) {
		// Obtain sample of next chunk if there is one
		if ((current_chunk_id_ + 1) < data_->chunks.size())
			next_chunk_sample_ = data_->chunks[current_chunk_id_ + 1].sample;
		else
			next_chunk_sample_ = std::numeric_limits<uint64_t>::max();
	}

	current_offset_++;
	current_chunk_offset_++;

	if (current_offset_ > data_size_) {
		qWarning() << "QHexView::get_next_byte() overran binary data boundary:" <<
			current_offset_ << "of" << data_size_ << "bytes";
		return 0xEE;
	}

	if ((current_chunk_offset_ == current_chunk_.data.size()) && (current_offset_ < data_size_)) {
		current_chunk_id_++;
		current_chunk_offset_ = 0;
		current_chunk_ = data_->chunks[current_chunk_id_];
	}

	return v;
}

QSize QHexView::getFullSize() const
{
	size_t width = posAscii_ + (BYTES_PER_LINE * charWidth_);

	if (verticalScrollBar()->isEnabled())
		width += GAP_ASCII_SLIDER + verticalScrollBar()->width();

	if (!data_ || (data_size_ == 0))
		return QSize(width, 0);

	size_t height = data_size_ / BYTES_PER_LINE;

	if (data_size_ % BYTES_PER_LINE)
		height++;

	height *= charHeight_;

	return QSize(width, height);
}

void QHexView::paintEvent(QPaintEvent *event)
{
	QPainter painter(viewport());

	QFont normal_font = painter.font();
	QFont bold_font = painter.font();
	bold_font.setWeight(QFont::Bold);

	bool bold_font_was_used = false;

	// Calculate and update the widget and paint area sizes
	QSize widgetSize = getFullSize();
	setMinimumWidth(widgetSize.width());
	setMaximumWidth(widgetSize.width());
	QSize areaSize = viewport()->size() - QSize(0, charHeight_);

	// Only show scrollbar if the content goes beyond the visible area
	if (widgetSize.height() > areaSize.height()) {
		verticalScrollBar()->setEnabled(true);
		verticalScrollBar()->setPageStep(areaSize.height() / charHeight_);
		verticalScrollBar()->setRange(0, ((widgetSize.height() - areaSize.height())) / charHeight_ + 1);
	} else
		verticalScrollBar()->setEnabled(false);

	// Fill widget background
	painter.fillRect(event->rect(), palette().color(QPalette::Base));

	if (!data_ || (data_size_ == 0) || (data_->chunks.empty())) {
		painter.setPen(palette().color(QPalette::Text));
		QString s = tr("No data available");
		int x = (areaSize.width() - fontMetrics().boundingRect(s).width()) / 2;
		int y = areaSize.height() / 2;
		painter.drawText(x, y, s);
		return;
	}

	// Determine first/last line indices
	size_t firstLineIdx = verticalScrollBar()->value();

	size_t lastLineIdx = firstLineIdx + (areaSize.height() / charHeight_);
	if (lastLineIdx > (data_size_ / BYTES_PER_LINE)) {
		lastLineIdx = data_size_ / BYTES_PER_LINE;
		if (data_size_ % BYTES_PER_LINE)
			lastLineIdx++;
	}

	// Paint divider line between hex and ASCII areas
	int line_x = posAscii_ - (GAP_HEX_ASCII / 2);
	painter.setPen(palette().color(QPalette::Midlight));
	painter.drawLine(line_x, event->rect().top(), line_x, height());

	// Fill address area background
	painter.fillRect(QRect(posAddr_, event->rect().top(),
		posHex_ - (GAP_ADR_HEX / 2), height()), palette().color(QPalette::Window));
	painter.fillRect(QRect(posAddr_, event->rect().top(),
		posAscii_ - (GAP_HEX_ASCII / 2), charHeight_ + 2), palette().color(QPalette::Window));

	// Paint address area
	painter.setPen(palette().color(QPalette::ButtonText));

	int yStart = 2 * charHeight_;
	for (size_t lineIdx = firstLineIdx, y = yStart; lineIdx < lastLineIdx; lineIdx++) {

		QString address = QString("%1").arg(lineIdx * 16, address_digits_, 16, QChar('0')).toUpper();
		painter.drawText(posAddr_, y, address);
		y += charHeight_;
	}

	// Paint top row with hex offsets
	painter.setPen(palette().color(QPalette::ButtonText));
	for (int offset = 0; offset <= 0xF; offset++)
		painter.drawText(posHex_ + (1 + offset * 3) * charWidth_,
			charHeight_ - 3, QString::number(offset, 16).toUpper());

	// Paint hex values
	QBrush regular_brush = palette().buttonText();
	QBrush selected_brush = palette().highlight();
	QBrush visible_range_brush = QBrush(visible_range_color_);

	bool multiple_chunks = (data_->chunks.size() > 1);
	unsigned int chunk_color = 0;

	initialize_byte_iterator(firstLineIdx * BYTES_PER_LINE);

	yStart = 2 * charHeight_;
	for (size_t lineIdx = firstLineIdx, y = yStart; lineIdx < lastLineIdx; lineIdx++) {

		int x = posHex_;
		for (size_t i = 0; (i < BYTES_PER_LINE) && (current_offset_ < data_size_); i++) {
			size_t pos = (lineIdx * BYTES_PER_LINE + i) * 2;

			// Fetch byte
			bool is_new_chunk;
			uint8_t byte_value = get_next_byte(&is_new_chunk);

			if (is_new_chunk) {
				chunk_color++;
				if (chunk_color == chunk_colors_.size())
					chunk_color = 0;

				// New chunk means also new chunk sample, so check for required changes
				if (bold_font_was_used)
					painter.setFont(normal_font);
				if ((highlighted_sample_ >= current_chunk_sample_) && (highlighted_sample_ < next_chunk_sample_)) {
					painter.setFont(bold_font);
					bold_font_was_used = true;
				}
			}

			// Restore default paint style
			painter.setBackground(regular_brush);
			painter.setBackgroundMode(Qt::TransparentMode);
			if (!multiple_chunks)
				painter.setPen(palette().color(QPalette::Text));
			else
				painter.setPen(chunk_colors_[chunk_color]);

			// Highlight needed because it's the range visible in main view?
			if ((current_chunk_sample_ >= visible_range_.first) && (current_chunk_sample_ < visible_range_.second)) {
				painter.setBackgroundMode(Qt::OpaqueMode);
				painter.setBackground(visible_range_brush);
			}

			// Highlight for selection range needed? (takes priority over visible range highlight)
			if ((pos >= selectBegin_) && (pos < selectEnd_)) {
				painter.setBackgroundMode(Qt::OpaqueMode);
				painter.setBackground(selected_brush);
				painter.setPen(palette().color(QPalette::HighlightedText));
			}

			// First nibble
			QString val = QString::number((byte_value & 0xF0) >> 4, 16).toUpper();
			painter.drawText(x, y, val);

			// Second nibble
			val = QString::number((byte_value & 0xF), 16).toUpper();
			painter.drawText(x + charWidth_, y, val);

			if ((i < BYTES_PER_LINE - 1) && (current_offset_ < data_size_))
				painter.drawText(x + 2 * charWidth_, y, QString(' '));

			x += 3 * charWidth_;
		}

		y += charHeight_;
	}

	// Paint ASCII characters
	initialize_byte_iterator(firstLineIdx * BYTES_PER_LINE);
	yStart = 2 * charHeight_;
	for (size_t lineIdx = firstLineIdx, y = yStart; lineIdx < lastLineIdx; lineIdx++) {

		int x = posAscii_;
		for (size_t i = 0; (i < BYTES_PER_LINE) && (current_offset_ < data_size_); i++) {
			// Fetch byte
			bool is_new_chunk;
			uint8_t ch = get_next_byte(&is_new_chunk);

			if (is_new_chunk) {
				// New chunk means also new chunk sample, so check for required changes
				if (bold_font_was_used)
					painter.setFont(normal_font);
				if ((highlighted_sample_ >= current_chunk_sample_) && (highlighted_sample_ < next_chunk_sample_)) {
					painter.setFont(bold_font);
					bold_font_was_used = true;
				}
			}

			if ((ch < 0x20) || (ch > 0x7E))
				ch = '.';

			// Restore default paint style
			painter.setBackgroundMode(Qt::TransparentMode);
			painter.setBackground(regular_brush);
			painter.setPen(palette().color(QPalette::Text));

			// Highlight needed because it's the range visible in main view?
			if ((current_chunk_sample_ >= visible_range_.first) && (current_chunk_sample_ < visible_range_.second)) {
				painter.setBackgroundMode(Qt::OpaqueMode);
				painter.setBackground(visible_range_brush);
			}

			// Highlight for selection range needed? (takes priority over visible range highlight)
			size_t pos = (lineIdx * BYTES_PER_LINE + i) * 2;
			if ((pos >= selectBegin_) && (pos < selectEnd_)) {
				painter.setBackgroundMode(Qt::OpaqueMode);
				painter.setBackground(selected_brush);
				painter.setPen(palette().color(QPalette::HighlightedText));
			}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			painter.drawText(x, y, QString(QChar(ch)));
#else
			painter.drawText(x, y, QString(ch));
#endif
			x += charWidth_;
		}

		y += charHeight_;
	}

	// Restore painter defaults
	painter.setBackgroundMode(Qt::TransparentMode);
	painter.setBackground(regular_brush);

	// Paint cursor
	if (hasFocus()) {
		int x = (cursorPos_ % (2 * BYTES_PER_LINE));
		int y = cursorPos_ / (2 * BYTES_PER_LINE);
		y -= firstLineIdx;
		int cursorX = (((x / 2) * 3) + (x % 2)) * charWidth_ + posHex_;
		int cursorY = charHeight_ + y * charHeight_ + 4;
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
		setCursorPos(data_size_ * 2);
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
		setSelection(2 * data_size_);
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
		int pos = data_size_ * 2;
		setCursorPos(pos);
		setSelection(pos);
		setVisible = true;
	}
	if (event->matches(QKeySequence::SelectStartOfDocument)) {
		setCursorPos(0);
		setSelection(0);
		setVisible = true;
	}

	if (event->matches(QKeySequence::Copy) && (data_)) {
		QString text;

		initialize_byte_iterator(selectBegin_ / 2);

		size_t selectedSize = (selectEnd_ - selectBegin_ + 1) / 2;
		for (size_t i = 0; i < selectedSize; i++) {
			uint8_t byte_value = get_next_byte();

			QString s = QString::number((byte_value & 0xF0) >> 4, 16).toUpper() +
				QString::number((byte_value & 0xF), 16).toUpper() + " ";
			text += s;

			if (i % BYTES_PER_LINE == (BYTES_PER_LINE - 1))
				text += "\n";
		}

		QClipboard *clipboard = QApplication::clipboard();
		clipboard->setText(text, QClipboard::Clipboard);
		if (clipboard->supportsSelection())
			clipboard->setText(text, QClipboard::Selection);
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
	size_t pos = -1;

	if (((size_t)position.x() >= posHex_) &&
		((size_t)position.x() < (posHex_ + HEXCHARS_IN_LINE * charWidth_))) {

		// Note: We add 1.5 character widths so that selection across
		// byte gaps is smoother
		size_t x = (position.x() + (1.5 * charWidth_ / 2) - posHex_) / charWidth_;

		// Note: We allow only full bytes to be selected, not nibbles,
		// so we round to the nearest byte gap
		x = (2 * x + 1) / 3;

		size_t firstLineIdx = verticalScrollBar()->value();
		size_t y = ((position.y() / charHeight_) - 1) * 2 * BYTES_PER_LINE;
		pos = x + y + firstLineIdx * BYTES_PER_LINE * 2;
	}

	size_t max_pos = data_size_ * 2;

	return std::min(pos, max_pos);
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

	int max_pos = data_size_ * 2;

	if (position > max_pos)
		position = max_pos;

	cursorPos_ = position;
}

void QHexView::ensureVisible()
{
	QSize areaSize = viewport()->size();

	int firstLineIdx = verticalScrollBar()->value();
	int lastLineIdx = firstLineIdx + areaSize.height() / charHeight_;

	int cursorY = cursorPos_ / (2 * BYTES_PER_LINE);

	if (cursorY < firstLineIdx)
		verticalScrollBar()->setValue(cursorY);
	else
		if(cursorY >= lastLineIdx)
			verticalScrollBar()->setValue(cursorY - areaSize.height() / charHeight_ + 1);
}
