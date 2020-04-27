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

#include "metadata_obj.hpp"


namespace pv {

const char* MetadataObjectNames[MetadataObjTypeCount] = {
	"main_view_range",
	"selection",
	"time_marker"
};

const char* MetadataValueNames[MetadataObjValueCount] = {
	"start_sample",
	"end_sample"
	"text"
};


MetadataObject::MetadataObject(MetadataObjManager* obj_manager, uint32_t obj_id,
	MetadataObjectType obj_type) :
	obj_manager_(obj_manager),
	id_(obj_id),
	type_(obj_type)
{
	// Make sure we accept all value type indices
	values_.resize(MetadataObjValueCount);
}

uint32_t MetadataObject::id() const
{
	return id_;
}

MetadataObjectType MetadataObject::type() const
{
	return type_;
}

void MetadataObject::set_value(MetadataValueType value_type, QVariant& value)
{
	values_.at((uint8_t)value_type) = value;
	obj_manager_->notify_observers(this, value_type);
}

QVariant MetadataObject::value(MetadataValueType value_type) const
{
	return values_.at((uint8_t)value_type);
}


MetadataObject* MetadataObjManager::create_object(MetadataObjectType obj_type)
{
	// Note: This function is not reentrant as race conditions between
	// emplace_back() and back() may occur

	objects_.emplace_back(this, objects_.size(), obj_type);
	MetadataObject* obj = &(objects_.back());

	for (MetadataObjObserverInterface *cb : callbacks_)
		cb->on_metadata_object_created(obj->id(), obj->type());

	return obj;
}

void MetadataObjManager::delete_object(uint32_t obj_id)
{
	MetadataObjectType type = objects_.at(obj_id).type();

	objects_.erase(std::remove_if(objects_.begin(), objects_.end(),
		[&](MetadataObject obj) { return obj.id() == obj_id; }),
		objects_.end());

	for (MetadataObjObserverInterface *cb : callbacks_)
		cb->on_metadata_object_deleted(obj_id, type);
}

MetadataObject* MetadataObjManager::find_object_by_type(MetadataObjectType obj_type)
{
	for (MetadataObject& obj : objects_)
		if (obj.type() == obj_type)
			return &obj;

	return nullptr;
}

MetadataObject* MetadataObjManager::object(uint32_t obj_id)
{
	return &(objects_.at(obj_id));
}

void MetadataObjManager::add_observer(MetadataObjObserverInterface *cb)
{
	callbacks_.emplace_back(cb);
}

void MetadataObjManager::remove_observer(MetadataObjObserverInterface *cb)
{
	for (auto cb_it = callbacks_.begin(); cb_it != callbacks_.end(); cb_it++)
		if (*cb_it == cb) {
			callbacks_.erase(cb_it);
			break;
		}
}

void MetadataObjManager::save_objects(QSettings &settings) const
{
	(void)settings;
}

void MetadataObjManager::restore_objects(QSettings &settings)
{
	(void)settings;
}

void MetadataObjManager::notify_observers(MetadataObject* obj,
	MetadataValueType changed_value)
{
	for (MetadataObjObserverInterface *cb : callbacks_)
		cb->on_metadata_object_changed(obj->id(), obj->type(), changed_value,
			obj->value(changed_value));
}

} // namespace pv
