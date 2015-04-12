/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Marcus Comstedt <marcus@mc.pp.se>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#endif

#include <android/log.h>

#include <stdint.h>
#include <libsigrok/libsigrok.h>

#include "android/loghandler.hpp"

namespace pv {

int AndroidLogHandler::sr_callback(void *cb_data, int loglevel, const char *format, va_list args)
{
	static const int prio[] = {
		[SR_LOG_NONE] = ANDROID_LOG_SILENT,
		[SR_LOG_ERR] = ANDROID_LOG_ERROR,
		[SR_LOG_WARN] = ANDROID_LOG_WARN,
		[SR_LOG_INFO] = ANDROID_LOG_INFO,
		[SR_LOG_DBG] = ANDROID_LOG_DEBUG,
		[SR_LOG_SPEW] = ANDROID_LOG_VERBOSE,
	};
	int ret;

	/* This specific log callback doesn't need the void pointer data. */
	(void)cb_data;

	/* Only output messages of at least the selected loglevel(s). */
	if (loglevel > sr_log_loglevel_get())
		return SR_OK;

	if (loglevel < SR_LOG_NONE)
		loglevel = SR_LOG_NONE;
	else if (loglevel > SR_LOG_SPEW)
		loglevel = SR_LOG_SPEW;

	ret = __android_log_vprint(prio[loglevel], "sr", format, args);

	return ret;
}

int AndroidLogHandler::srd_callback(void *cb_data, int loglevel, const char *format, va_list args)
{
#ifdef ENABLE_DECODE
	static const int prio[] = {
		[SRD_LOG_NONE] = ANDROID_LOG_SILENT,
		[SRD_LOG_ERR] = ANDROID_LOG_ERROR,
		[SRD_LOG_WARN] = ANDROID_LOG_WARN,
		[SRD_LOG_INFO] = ANDROID_LOG_INFO,
		[SRD_LOG_DBG] = ANDROID_LOG_DEBUG,
		[SRD_LOG_SPEW] = ANDROID_LOG_VERBOSE,
	};
	int ret;

	/* This specific log callback doesn't need the void pointer data. */
	(void)cb_data;

	/* Only output messages of at least the selected loglevel(s). */
	if (loglevel > srd_log_loglevel_get())
		return SRD_OK;

	if (loglevel < SRD_LOG_NONE)
		loglevel = SRD_LOG_NONE;
	else if (loglevel > SRD_LOG_SPEW)
		loglevel = SRD_LOG_SPEW;

	ret = __android_log_vprint(prio[loglevel], "srd", format, args);

	return ret;
#else
	return 0;
#endif
}

void AndroidLogHandler::install_callbacks()
{
	sr_log_callback_set(sr_callback, nullptr);
#ifdef ENABLE_DECODE
	srd_log_callback_set(srd_callback, nullptr);
#endif
}

} // namespace pv
