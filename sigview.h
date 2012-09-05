/*
 * This file is part of the sigrok project.
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

#ifndef SIGVIEW_H
#define SIGVIEW_H

#include <stdint.h>

#include <QAbstractScrollArea>

class SigSession;
class SigViewport;

class SigView : public QAbstractScrollArea {
	Q_OBJECT

private:
	static const double MaxScale;
	static const double MinScale;

	static const int LabelMarginWidth;
	static const int RulerHeight;

public:
	explicit SigView(SigSession &session, QWidget *parent = 0);

	double scale() const;
	double offset() const;
	int v_offset() const;

	void zoom(double steps);

	void set_scale_offset(double scale, double offset);

private:
	void update_scroll();

	void zoom(double steps, int offset);

private:
	bool viewportEvent(QEvent *e);

	void resizeEvent(QResizeEvent *e);

private slots:
	void h_scroll_value_changed(int value);
	void v_scroll_value_changed(int value);

	void data_updated();

private:
	SigSession &_session;

	SigViewport *_viewport;

	uint64_t _data_length;

	double _scale;
	double _offset;

	int _v_offset;

	friend class SigViewport;
};

#endif // SIGVIEW_H
