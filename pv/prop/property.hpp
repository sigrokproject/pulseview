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

#ifndef PULSEVIEW_PV_PROP_PROPERTY_HPP
#define PULSEVIEW_PV_PROP_PROPERTY_HPP

#include <glib.h>
// Suppress warnings due to use of deprecated std::auto_ptr<> by glibmm.
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <glibmm.h>
G_GNUC_END_IGNORE_DEPRECATIONS

#include <functional>
#include <QString>
#include <QWidget>

class QWidget;

namespace pv {
namespace prop {

class Property : public QObject
{
	Q_OBJECT;

public:
	typedef std::function<Glib::VariantBase ()> Getter;
	typedef std::function<void (Glib::VariantBase)> Setter;

protected:
	Property(QString name, Getter getter, Setter setter);

public:
	const QString& name() const;

	virtual QWidget* get_widget(QWidget *parent,
		bool auto_commit = false) = 0;
	virtual bool labeled_widget() const;

	virtual void commit() = 0;

protected:
	const Getter getter_;
	const Setter setter_;

private:
	QString name_;
};

} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_PROPERTY_HPP
