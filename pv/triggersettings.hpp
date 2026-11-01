/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PULSEVIEW_PV_TRIGGERSETTINGS_HPP
#define PULSEVIEW_PV_TRIGGERSETTINGS_HPP

#include <memory>

class QSettings;
namespace sigrok {
class Context;
class Device;
class Session;
}

namespace pv {

void save_trigger_settings(QSettings &settings,
	const std::shared_ptr<sigrok::Session> &session);

/* Returns false for legacy setups without trigger settings. Invalid settings
 * throw before replacing the current trigger; an explicit disabled trigger
 * clears it. */
bool restore_trigger_settings(QSettings &settings,
	const std::shared_ptr<sigrok::Context> &context,
	const std::shared_ptr<sigrok::Device> &device,
	const std::shared_ptr<sigrok::Session> &session);

}
#endif
