/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Standalone allocation-failure regression. Global new/delete overrides are
 * confined to this executable; no production hook or process-wide OOM is used.
 * Real append/read/container methods run with small chunks to exercise deque
 * boundaries cheaply. Unsafe negative controls exit before dereferencing or
 * destroying retained storage twice. */
#include <pv/data/segment.hpp>
#include <QCoreApplication>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {
thread_local bool fail_array = false;
thread_local bool arm_after_array = false;
thread_local bool fail_object = false;
thread_local unsigned object_failures = 0;
thread_local unsigned fail_array_countdown = 0;
thread_local long live_arrays = 0;
thread_local void* retained_tail = nullptr;
thread_local bool retained_tail_deleted = false;

void* allocate(std::size_t size)
{
	if (void* p = std::malloc(size ? size : 1))
		return p;
	throw std::bad_alloc();
}
}

void* operator new(std::size_t size)
{
	if (fail_object) {
		fail_object = false;
		++object_failures;
		throw std::bad_alloc();
	}
	return allocate(size);
}

void* operator new[](std::size_t size)
{
	if (fail_array || (fail_array_countdown && --fail_array_countdown == 0)) {
		fail_array = false;
		throw std::bad_alloc();
	}
	void* p = allocate(size);
	++live_arrays;
	if (arm_after_array) {
		arm_after_array = false;
		fail_object = true;
	}
	return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept
{
	if (p == retained_tail && p)
		retained_tail_deleted = true;
	if (p)
		--live_arrays;
	std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete[](p); }

class SmallChunkSegment : public pv::data::Segment
{
public:
	SmallChunkSegment() : Segment(0, 1000000, 1)
	{
		// Keep normal append/read/container logic while making deque block
		// boundaries cheap to exercise. No production chunk size is changed.
		delete[] current_chunk_;
		chunk_size_ = 32;
		current_chunk_ = new uint8_t[chunk_size_ + 7];
		data_chunks_.back() = current_chunk_;
		unused_samples_ = chunk_size_;
	}
	using Segment::append_samples;
	using Segment::append_single_sample;
	using Segment::get_raw_samples;
	void* tail() const { return current_chunk_; }
	std::size_t chunks() const { return data_chunks_.size(); }
	bool reachable() const { return data_chunks_.back() == current_chunk_; }
	void recover_negative_fixture()
	{
		if (current_chunk_ && !reachable())
			delete[] current_chunk_;
		current_chunk_ = data_chunks_.back();
	}
};

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	unsigned failures = 0, cases = 0;
	for (unsigned chunks : {1, 63, 64, 65, 127, 128, 129, 1024}) {
		for (bool array_failure : {false, true}) {
			SmallChunkSegment segment;
			std::vector<uint8_t> input((chunks - 1) * 32 + 16);
			for (std::size_t i = 0; i < input.size(); ++i)
				input[i] = uint8_t((i * 29) ^ (i >> 3));
			segment.append_samples(input.data(), input.size());
			const unsigned before = object_failures;
			bool threw = false;
			fail_array = array_failure;
			arm_after_array = !array_failure;
			try { segment.free_unused_memory(); }
			catch (const std::bad_alloc&) { threw = true; }
			fail_array = arm_after_array = fail_object = false;
			bool good = segment.chunks() == chunks && segment.reachable() &&
				segment.get_sample_count() == input.size() &&
				threw == array_failure && object_failures == before;
			if (good) {
				std::vector<uint8_t> output(input.size());
				segment.get_raw_samples(0, output.size(), output.data());
				good = output == input;
			}
			if (!good) {
				++failures;
				std::fprintf(stderr, "chunks=%u array_failure=%d: ownership/data failure\n",
					chunks, array_failure);
			}
			// Negative controls must not dereference missing storage or leak
			// the orphan. Recovery is fixture cleanup after recording failure.
			segment.recover_negative_fixture();
			++cases;
		}
	}
	for (unsigned chunks : {1, 63, 127}) {
		for (bool single : {false, true}) {
			for (unsigned fault : {1, 2, 3}) {
				if (single && fault == 2)
					continue; // The single-sample path has no reserve array.
				SmallChunkSegment segment;
				std::vector<uint8_t> input((chunks - 1) * 32 + (single ? 31 : 16));
				for (std::size_t i = 0; i < input.size(); ++i)
					input[i] = uint8_t(i * 37 + 11);
				segment.append_samples(input.data(), input.size());
				const std::vector<uint8_t> added(single ? 1 : 16, 0xa5);
				const long arrays_before = live_arrays;
				retained_tail = segment.tail();
				retained_tail_deleted = false;
				fail_array_countdown = fault == 3 ? 0 : fault;
				arm_after_array = fault == 3;
				bool threw = false;
				try {
					if (single) {
						uint8_t value = added[0];
						segment.append_single_sample(&value);
					} else
						segment.append_samples(const_cast<uint8_t*>(added.data()), added.size());
				} catch (const std::bad_alloc&) { threw = true; }
				fail_array_countdown = 0;
				arm_after_array = fail_object = false;
				retained_tail = nullptr;
				bool good = !retained_tail_deleted &&
					segment.reachable() && segment.chunks() == chunks + (threw ? 0 : 1) &&
					live_arrays == arrays_before + (threw ? 0 : 1) &&
					segment.get_sample_count() == input.size() + (threw ? 0 : added.size());
				if (!good) {
					std::fprintf(stderr, "append chunks=%u single=%d fault=%u: retained storage deleted or new chunk leaked\n",
						chunks, single, fault);
					// A negative control may contain a dangling owned pointer.
					// Exit before reading it or invoking a second delete.
					std::fflush(nullptr);
					std::_Exit(EXIT_FAILURE);
				}
				if (!threw)
					input.insert(input.end(), added.begin(), added.end());
				std::vector<uint8_t> output(input.size());
				segment.get_raw_samples(0, output.size(), output.data());
				if (output != input)
					++failures;
				++cases;
			}
		}
	}
	std::printf("%u compaction/append cases, %u failures\n", cases, failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
