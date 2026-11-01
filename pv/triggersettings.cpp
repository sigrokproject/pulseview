/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "triggersettings.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <QSettings>
#include <QVariant>
#include <libsigrokcxx/libsigrokcxx.hpp>

namespace pv {

void save_trigger_settings(QSettings &settings,
	const std::shared_ptr<sigrok::Session> &session)
{
	if (!session)
		return;
	const auto trigger = session->trigger();
	settings.setValue("trigger/version", 1);
	settings.setValue("trigger/enabled", !!trigger);
	settings.setValue("trigger/name", trigger ?
		QString::fromStdString(trigger->name()) : QString());
	QVariantList stages;
	if (trigger) {
		for (const auto &stage : trigger->stages()) {
			QVariantList matches;
			for (const auto &match : stage->matches()) {
				QVariantMap item;
				item["channel"] = match->channel()->index();
				item["channel_type"] = match->channel()->type()->id();
				item["type"] = match->type()->id();
				if (!std::isnan(match->value()))
					item["value"] = match->value();
				matches.append(item);
			}
			QVariantMap item;
			item["matches"] = matches;
			stages.append(item);
		}
	}
	settings.setValue("trigger/stages", stages);
}

bool restore_trigger_settings(QSettings &settings,
	const std::shared_ptr<sigrok::Context> &context,
	const std::shared_ptr<sigrok::Device> &device,
	const std::shared_ptr<sigrok::Session> &session)
{
	if (!settings.contains("trigger/version"))
		return false;
	if (settings.value("trigger/version").toInt() != 1 ||
			!settings.contains("trigger/enabled") || !context || !device || !session)
		throw std::runtime_error("Invalid trigger setup or no capture device.");
	const QString enabled = settings.value("trigger/enabled").toString();
	if (enabled != "true" && enabled != "false")
		throw std::runtime_error("Invalid trigger enabled state.");
	if (enabled == "false") {
		session->set_trigger(nullptr);
		return true;
	}
	const auto stages = settings.value("trigger/stages").toList();
	if (stages.empty())
		throw std::runtime_error("Trigger setup has no stages.");
	if (!device->config_check(sigrok::ConfigKey::TRIGGER_MATCH, sigrok::Capability::LIST))
		throw std::runtime_error("Device does not list supported trigger conditions.");
	auto supported = device->config_list(sigrok::ConfigKey::TRIGGER_MATCH);
	gsize size;
	const auto types = static_cast<const int32_t *>(
		g_variant_get_fixed_array(supported.gobj(), &size, sizeof(int32_t)));
	if (!size)
		throw std::runtime_error("Device lists no supported trigger conditions.");
	const auto channels = device->channels();
	const auto next = context->create_trigger(settings.value("trigger/name").toString().toStdString());
	for (const auto &stage : stages) {
		const auto matches = stage.toMap().value("matches").toList();
		if (matches.empty())
			throw std::runtime_error("Trigger setup has an empty stage.");
		const auto next_stage = next->add_stage();
		for (const auto &match : matches) {
			const auto item = match.toMap();
			bool channel_ok, type_ok, channel_type_ok;
			const auto index = item.value("channel").toUInt(&channel_ok);
			const int type = item.value("type").toInt(&type_ok);
			const int channel_type = item.value("channel_type").toInt(&channel_type_ok);
			const auto channel = std::find_if(channels.begin(), channels.end(),
				[index](const std::shared_ptr<sigrok::Channel> &c) { return c->index() == index; });
			if (!channel_ok || !type_ok || !channel_type_ok || channel == channels.end() ||
					(*channel)->type()->id() != channel_type ||
					std::find(types, types + size, type) == types + size)
				throw std::runtime_error("Unsupported trigger channel or condition.");
			float value = NAN;
			if (item.contains("value")) {
				bool value_ok;
				value = item.value("value").toFloat(&value_ok);
				if (!value_ok || !std::isfinite(value))
					throw std::runtime_error("Invalid trigger threshold value.");
			}
			next_stage->add_match(*channel, sigrok::TriggerMatchType::get(type), value);
		}
	}
	// All stages have been validated before changing the live session.
	session->set_trigger(next);
	return true;
}

}
