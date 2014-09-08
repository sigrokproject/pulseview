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

#ifndef PULSEVIEW_PV_VIEW_ROWITEMITERATOR_H
#define PULSEVIEW_PV_VIEW_ROWITEMITERATOR_H

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <stack>
#include <type_traits>
#include <vector>

#include <boost/thread.hpp>

#include <pv/sigsession.h>

namespace pv {
namespace view {

template<class Owner, class Item> class RowItemIterator
{
public:
	typedef typename std::conditional<std::is_const<Owner>::value,
		typename Owner::item_list::const_iterator,
		typename Owner::item_list::iterator>::type child_iterator;

	typedef std::shared_ptr<Item> value_type;
	typedef ptrdiff_t difference_type;
	typedef value_type pointer;
	typedef value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

public:
	RowItemIterator(Owner *owner) :
		_owner(owner),
		_lock(owner->session().signals_mutex()) {}

	RowItemIterator(Owner *owner, child_iterator iter) :
		_owner(owner),
		_lock(owner->session().signals_mutex()) {
		assert(owner);
		if (iter != owner->child_items().end())
			_iter_stack.push(iter);
	}

	RowItemIterator(const RowItemIterator<Owner, Item> &o) :
		_owner(o._owner),
		_lock(*o._lock.mutex()),
		_iter_stack(o._iter_stack) {}

	reference operator*() const {
		return *_iter_stack.top();
	}

	reference operator->() const {
		return *this;
	}

	RowItemIterator<Owner, Item>& operator++() {
		using std::dynamic_pointer_cast;
		using std::shared_ptr;

		assert(_owner);
		assert(!_iter_stack.empty());

		shared_ptr<Owner> owner(dynamic_pointer_cast<Owner>(
			*_iter_stack.top()));
		if (owner && !owner->child_items().empty()) {
			_owner = owner.get();
			_iter_stack.push(owner->child_items().begin());
		} else {
			++_iter_stack.top();
			while (_owner && _iter_stack.top() ==
				_owner->child_items().end()) {
				_iter_stack.pop();
				_owner = _iter_stack.empty() ? nullptr :
					(*_iter_stack.top()++)->owner();
			}
		}

		return *this;
	}

	RowItemIterator<Owner, Item> operator++(int) {
		RowItemIterator<Owner, Item> pre = *this;
		++*this;
		return pre;
	}

	bool operator==(const RowItemIterator &o) const {
		return (_iter_stack.empty() && o._iter_stack.empty()) ||
			(_owner == o._owner &&
			_iter_stack.size() == o._iter_stack.size() &&
			std::equal(
				_owner->child_items().cbegin(),
				_owner->child_items().cend(),
				o._owner->child_items().cbegin()));
	}

	bool operator!=(const RowItemIterator &o) const {
		return !((const RowItemIterator&)*this == o);
	}

	void swap(RowItemIterator<Owner, Item>& other) {
		swap(_owner, other._owner);
		swap(_iter_stack, other._iter_stack);
	}

private:
	Owner *_owner;
	boost::shared_lock<boost::shared_mutex> _lock;
	std::stack<child_iterator> _iter_stack;
};

template<class Owner, class Item>
void swap(RowItemIterator<Owner, Item>& a, RowItemIterator<Owner, Item>& b)
{
	a.swap(b);
}

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_ROWITEMITERATOR_H
