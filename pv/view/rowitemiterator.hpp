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

#ifndef PULSEVIEW_PV_VIEW_ROWITEMITERATOR_HPP
#define PULSEVIEW_PV_VIEW_ROWITEMITERATOR_HPP

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <stack>
#include <type_traits>
#include <vector>

#ifdef _WIN32
// Windows: Avoid namespace pollution by thread.hpp (which includes windows.h).
#define NOGDI
#define NORESOURCE
#endif
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <pv/session.hpp>

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
		owner_(owner),
		lock_(owner->session().signals_mutex()) {}

	RowItemIterator(Owner *owner, child_iterator iter) :
		owner_(owner),
		lock_(owner->session().signals_mutex()) {
		assert(owner);
		if (iter != owner->child_items().end())
			iter_stack_.push(iter);
	}

	RowItemIterator(const RowItemIterator<Owner, Item> &o) :
		owner_(o.owner_),
		lock_(*o.lock_.mutex()),
		iter_stack_(o.iter_stack_) {}

	reference operator*() const {
		return *iter_stack_.top();
	}

	reference operator->() const {
		return *this;
	}

	RowItemIterator<Owner, Item>& operator++() {
		using std::dynamic_pointer_cast;
		using std::shared_ptr;

		assert(owner_);
		assert(!iter_stack_.empty());

		shared_ptr<Owner> owner(dynamic_pointer_cast<Owner>(
			*iter_stack_.top()));
		if (owner && !owner->child_items().empty()) {
			owner_ = owner.get();
			iter_stack_.push(owner->child_items().begin());
		} else {
			++iter_stack_.top();
			while (owner_ && iter_stack_.top() ==
				owner_->child_items().end()) {
				iter_stack_.pop();
				owner_ = iter_stack_.empty() ? nullptr :
					(*iter_stack_.top()++)->owner();
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
		return (iter_stack_.empty() && o.iter_stack_.empty()) ||
			(owner_ == o.owner_ &&
			iter_stack_.size() == o.iter_stack_.size() &&
			std::equal(
				owner_->child_items().cbegin(),
				owner_->child_items().cend(),
				o.owner_->child_items().cbegin()));
	}

	bool operator!=(const RowItemIterator &o) const {
		return !((const RowItemIterator&)*this == o);
	}

	void swap(RowItemIterator<Owner, Item>& other) {
		swap(owner_, other.owner_);
		swap(iter_stack_, other.iter_stack_);
	}

private:
	Owner *owner_;
	boost::shared_lock<boost::shared_mutex> lock_;
	std::stack<child_iterator> iter_stack_;
};

template<class Owner, class Item>
void swap(RowItemIterator<Owner, Item>& a, RowItemIterator<Owner, Item>& b)
{
	a.swap(b);
}

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_ROWITEMITERATOR_HPP
