/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_VIEW_VIEWITEMITERATOR_HPP
#define PULSEVIEW_PV_VIEW_VIEWITEMITERATOR_HPP

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <stack>
#include <type_traits>
#include <vector>

#include <pv/session.hpp>

namespace pv {
namespace view {

template<class Owner, class Item> class ViewItemIterator
{
public:
	typedef typename Owner::item_list::const_iterator child_iterator;
	typedef std::shared_ptr<Item> value_type;
	typedef ptrdiff_t difference_type;
	typedef value_type pointer;
	typedef const value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

public:
	ViewItemIterator(Owner *owner) :
		owner_stack_({owner}) {}

	ViewItemIterator(Owner *owner, child_iterator iter) :
		owner_stack_({owner}) {
		assert(owner);
		if (iter != owner->child_items().end())
			iter_stack_.push(iter);
	}

	ViewItemIterator(const ViewItemIterator<Owner, Item> &o) :
		owner_stack_(o.owner_stack_),
		iter_stack_(o.iter_stack_) {}

	reference operator*() const {
		return *iter_stack_.top();
	}

	reference operator->() const {
		return *this;
	}

	ViewItemIterator<Owner, Item>& operator++() {
		using std::dynamic_pointer_cast;
		using std::shared_ptr;

		assert(!owner_stack_.empty());
		assert(!iter_stack_.empty());

		shared_ptr<Owner> owner(dynamic_pointer_cast<Owner>(
			*iter_stack_.top()));
		if (owner && !owner->child_items().empty()) {
			owner_stack_.push(owner.get());
			iter_stack_.push(owner->child_items().begin());
		} else {
			while (!iter_stack_.empty() && (++iter_stack_.top()) ==
				owner_stack_.top()->child_items().end()) {
				owner_stack_.pop();
				iter_stack_.pop();
			}
		}

		return *this;
	}

	ViewItemIterator<Owner, Item> operator++(int) {
		ViewItemIterator<Owner, Item> pre = *this;
		++*this;
		return pre;
	}

	bool operator==(const ViewItemIterator &o) const {
		return (iter_stack_.empty() && o.iter_stack_.empty()) || (
			iter_stack_.size() == o.iter_stack_.size() &&
			owner_stack_.top() == o.owner_stack_.top() &&
			iter_stack_.top() == o.iter_stack_.top());
	}

	bool operator!=(const ViewItemIterator &o) const {
		return !((const ViewItemIterator&)*this == o);
	}

	void swap(ViewItemIterator<Owner, Item>& other) {
		swap(owner_stack_, other.owner_stack_);
		swap(iter_stack_, other.iter_stack_);
	}

private:
	std::stack<Owner*> owner_stack_;
	std::stack<child_iterator> iter_stack_;
};

template<class Owner, class Item>
void swap(ViewItemIterator<Owner, Item>& a, ViewItemIterator<Owner, Item>& b)
{
	a.swap(b);
}

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_VIEWITEMITERATOR_HPP
