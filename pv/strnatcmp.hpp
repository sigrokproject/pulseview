/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2000, 2004 by Martin Pool <mbp sourcefrog net>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

// strnatcmp.c -- Perform 'natural order' comparisons of strings in C.
// This file has been modified for C++ compatibility.
// Original at https://github.com/sourcefrog/natsort/blob/master/strnatcmp.c

#ifndef PULSEVIEW_PV_STRNATCMP_HPP
#define PULSEVIEW_PV_STRNATCMP_HPP

#include <stddef.h>	/* size_t */
#include <ctype.h>
#include <string>

using std::string;

static int compare_right(char const *a, char const *b)
{
	int bias = 0;

	// The longest run of digits wins. That aside, the greatest
	// value wins, but we can't know that it will until we've scanned
	// both numbers to know that they have the same magnitude, so we
	// remember it in bias.
	for (;; a++, b++) {
		if (!isdigit(*a) && !isdigit(*b))
			return bias;
		if (!isdigit(*a))
			return -1;
		if (!isdigit(*b))
			return +1;

		if (*a < *b) {
			if (!bias)
				bias = -1;
		} else if (*a > *b) {
			if (!bias)
				bias = +1;
		} else if (!*a && !*b)
			return bias;
	}

	return 0;
}

static int compare_left(char const *a, char const *b)
{
	// Compare two left-aligned numbers: the first to have a
	// different value wins.
	for (;; a++, b++) {
		if (!isdigit(*a)  &&  !isdigit(*b))
			return 0;
		if (!isdigit(*a))
			return -1;
		if (!isdigit(*b))
			return +1;
		if (*a < *b)
			return -1;
		if (*a > *b)
			return +1;
	}

	return 0;
}

static int strnatcmp0(char const *a, char const *b, int fold_case)
{
	int ai, bi, fractional, result;
	char ca, cb;

	ai = bi = 0;

	while (1) {
		ca = a[ai];
		cb = b[bi];

		// Skip over leading spaces or zeroes
		while (isspace(ca))
			ca = a[++ai];

		while (isspace(cb))
			cb = b[++bi];

		// Process run of digits
		if (isdigit(ca) && isdigit(cb)) {
			fractional = (ca == '0' || cb == '0');

			if (fractional) {
				if ((result = compare_left(a + ai, b + bi)) != 0)
					return result;
			} else {
				if ((result = compare_right(a + ai, b + bi)) != 0)
					return result;
			}
		}

		if (!ca && !cb) {
			// The strings compare the same. Perhaps the caller
			// will want to call strcmp to break the tie
			return 0;
		}

		if (fold_case) {
			ca = toupper(ca);
			cb = toupper(cb);
		}

		if (ca < cb)
			return -1;

		if (ca > cb)
			return +1;

		++ai;
		++bi;
	}
}

// Compare, recognizing numeric strings and being case sensitive
int strnatcmp(char const *a, char const *b)
{
	return strnatcmp0(a, b, 0);
}

int strnatcmp(const string a, const string b)
{
	return strnatcmp0(a.c_str(), b.c_str(), 0);
}

// Compare, recognizing numeric strings and ignoring case
int strnatcasecmp(char const *a, char const *b)
{
	return strnatcmp0(a, b, 1);
}

int strnatcasecmp(const string a, const string b)
{
	return strnatcmp0(a.c_str(), b.c_str(), 1);
}

#endif // PULSEVIEW_PV_STRNATCMP_HPP
