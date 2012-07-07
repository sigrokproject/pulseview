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

#include <QtOpenGL/QGLWidget>
#include <QTimer>

class QPainter;
class QPaintEvent;
class SigSession;

class SigView : public QGLWidget
{
	Q_OBJECT

private:
	static const double MaxScale;
	static const double MinScale;

	static const int SignalHeight;
	static const int LabelMarginWidth;
	static const int RulerHeight;

	static const int MinorTickSubdivision;
	static const int ScaleUnits[3];

	static const QString SIPrefixes[9];
	static const int FirstSIPrefixPower;

public:
	explicit SigView(SigSession &session, QWidget *parent = 0);

protected:

	void initializeGL();

	void resizeGL(int width, int height);

	void paintEvent(QPaintEvent *event);

private:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);

private:
	void setup_viewport(int width, int height);

	void paint_ruler(QPainter &p);

private slots:
	void data_updated();

private:
	SigSession &_session;

	double _scale;
	double _offset;

	QPoint _mouse_down_point;
	double _mouse_down_offset;
};

#endif // SIGVIEW_H
