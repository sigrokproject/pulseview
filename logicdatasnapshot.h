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

#include "datasnapshot.h"

#include <utility>
#include <vector>

namespace LogicDataSnapshotTest {
	class Basic;
	class LargeData;
}

class LogicDataSnapshot : public DataSnapshot
{
private:
	struct MipMapLevel
	{
		uint64_t length;
		uint64_t data_length;
		void *data;
	};

private:
	static const int ScaleStepCount = 10;
	static const int MipMapScalePower;
	static const int MipMapScaleFactor;
	static const float LogMipMapScaleFactor;
	static const uint64_t MipMapDataUnit;

public:
	typedef std::pair<int64_t, bool> EdgePair;

public:
	LogicDataSnapshot(const sr_datafeed_logic &logic);

	virtual ~LogicDataSnapshot();

	void append_payload(const sr_datafeed_logic &logic);

private:
	void reallocate_mip_map(MipMapLevel &m);

	void append_payload_to_mipmap();

public:
	uint64_t get_sample(uint64_t index) const;

	/**
	 * Parses a logic data snapshot to generate a list of transitions
	 * in a time interval to a given level of detail.
	 * @param[out] edges The vector to place the edges into.
	 * @param[in] start The start sample index.
	 * @param[in] end The end sample index.
	 * @param[in] min_length The minimum number of samples that
	 * can be resolved at this level of detail.
	 * @param[in] sig_index The index of the signal.
	 **/
	void get_subsampled_edges(std::vector<EdgePair> &edges,
		int64_t start, int64_t end,
		float min_length, int sig_index);

private:

	static inline int64_t pow2_ceil(int64_t x, int power);

private:
	struct MipMapLevel _mip_map[ScaleStepCount];
	uint64_t _last_append_sample;

	friend class LogicDataSnapshotTest::Basic;
	friend class LogicDataSnapshotTest::LargeData;
};
