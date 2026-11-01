/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PULSEVIEW_PV_DATA_SEGMENTMUTEX_HPP
#define PULSEVIEW_PV_DATA_SEGMENTMUTEX_HPP

#include <exception>
#include <mutex>
#include <type_traits>

namespace pv {
namespace data {

/* In MinGW's statically initialized libstdc++ mutex branch, the default
 * destructor is trivial. Winpthreads lazily allocates an event on first lock,
 * which still needs pthread_mutex_destroy. Restrict that extra destruction to
 * this implementation; other platforms retain standard destruction.
 * Segment lock guards release their locks before member destruction. */
class SegmentMutex final : public std::recursive_mutex {
public:
	SegmentMutex() = default;
	~SegmentMutex() noexcept
	{
#if defined(_WIN32) && defined(__GLIBCXX__) && defined(__GTHREAD_RECURSIVE_MUTEX_INIT)
		destroy_if_trivial(std::is_trivially_destructible<std::recursive_mutex>());
#endif
	}

private:
#if defined(_WIN32) && defined(__GLIBCXX__) && defined(__GTHREAD_RECURSIVE_MUTEX_INIT)
	void destroy_if_trivial(std::true_type) noexcept
	{
		if (__gthread_recursive_mutex_destroy(native_handle()) != 0)
			std::terminate();
	}
	void destroy_if_trivial(std::false_type) noexcept {}
#endif
};

} // namespace data
} // namespace pv

#endif
