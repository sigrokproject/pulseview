/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_METADATA_OBJ_HPP
#define PULSEVIEW_PV_METADATA_OBJ_HPP

#include <deque>
#include <vector>

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

using std::deque;
using std::vector;

namespace pv {


// When adding an entry here, don't forget to update MetadataObjectNames as well
enum MetadataObjectType {
	MetadataObjMainViewRange,
	MetadataObjSelection,
	MetadataObjTimeMarker,
	MetadataObjectTypeCount  // Indicates how many metadata object types there are, must always be last
};

// When adding an entry here, don't forget to update MetadataValueNames as well
enum MetadataValueType {
	MetadataValueStartSample,  // int64_t / qlonglong
	MetadataValueEndSample,    // int64_t / qlonglong
	MetadataValueText,
	MetadataValueTypeCount  // Indicates how many metadata value types there are, must always be last
};

extern const char* MetadataObjectNames[MetadataObjectTypeCount];
extern const char* MetadataValueNames[MetadataValueTypeCount];


class MetadataObjManager;
class MetadataObject;


class MetadataObjObserverInterface
{
public:
	virtual void on_metadata_object_created(MetadataObject* obj);
	virtual void on_metadata_object_deleted(MetadataObject* obj);
	virtual void on_metadata_object_changed(MetadataObject* obj,
		MetadataValueType value_type);
};


class MetadataObject
{
public:
	MetadataObject(MetadataObjManager* obj_manager, uint32_t obj_id, MetadataObjectType obj_type);
	virtual ~MetadataObject() = default;

	virtual uint32_t id() const;
	virtual MetadataObjectType type() const;

	virtual void set_value(MetadataValueType value_type, const QVariant& value);
	virtual QVariant value(MetadataValueType value_type) const;
private:
	MetadataObjManager* obj_manager_;
	uint32_t id_;
	MetadataObjectType type_;
	vector<QVariant> values_;
};


class MetadataObjManager : public QObject
{
	Q_OBJECT

public:
	MetadataObject* create_object(MetadataObjectType obj_type);
	void delete_object(uint32_t obj_id);
	MetadataObject* find_object_by_type(MetadataObjectType obj_type);
	MetadataObject* object(uint32_t obj_id);

	void add_observer(MetadataObjObserverInterface *cb);
	void remove_observer(MetadataObjObserverInterface *cb);

	void save_objects(QSettings &settings) const;
	void restore_objects(QSettings &settings);

	void notify_observers(MetadataObject* obj, MetadataValueType changed_value);

private:
	vector<MetadataObjObserverInterface*> callbacks_;
	deque<MetadataObject> objects_;
};


} // namespace pv

#endif // PULSEVIEW_PV_METADATA_OBJ_HPP
