/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Real QSettings round trips and libsigrok sessions; demo driver only. */
#include "test/test.hpp"
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <stdexcept>
#include <QSettings>
#include <QTemporaryDir>
#include <libsigrokcxx/libsigrokcxx.hpp>
#include <pv/triggersettings.hpp>

namespace {

struct Fixture {
	std::shared_ptr<sigrok::Context> context = sigrok::Context::create();
	std::shared_ptr<sigrok::HardwareDevice> device;
	std::shared_ptr<sigrok::Session> session;
	QTemporaryDir directory;

	Fixture()
	{
		device = context->drivers().at("demo")->scan({
			{sigrok::ConfigKey::NUM_LOGIC_CHANNELS, Glib::Variant<gint32>::create(32)},
			{sigrok::ConfigKey::NUM_ANALOG_CHANNELS, Glib::Variant<gint32>::create(1)},
		}).at(0);
		device->open();
		session = context->create_session();
		session->add_device(device);
	}

	~Fixture()
	{
		session->remove_devices();
		session.reset();
		device->close();
	}

	QString filename() const { return directory.filePath("trigger.ini"); }

	std::shared_ptr<sigrok::Trigger> trigger(unsigned int channel = 0)
	{
		auto result = context->create_trigger("test");
		result->add_stage()->add_match(device->channels().at(channel), sigrok::TriggerMatchType::RISING);
		return result;
	}
};

std::string snapshot(const std::shared_ptr<sigrok::Trigger> &trigger)
{
	if (!trigger)
		return "none";
	std::string result = trigger->name();
	for (const auto &stage : trigger->stages()) {
		result += "/stage";
		for (const auto &match : stage->matches()) {
			result += ":" + std::to_string(match->channel()->index()) +
				"," + std::to_string(match->type()->id()) + "," +
				(std::isnan(match->value()) ? "nan" : std::to_string(match->value()));
		}
	}
	return result;
}

}

BOOST_AUTO_TEST_SUITE(TriggerSettings)

BOOST_AUTO_TEST_CASE(logic_roundtrip)
{
	Fixture f;
	for (unsigned int channel : {0, 7, 15, 31}) {
		auto trigger = f.trigger(channel);
		trigger->stages().at(0)->add_match(f.device->channels().at((channel + 1) % 32),
			sigrok::TriggerMatchType::FALLING);
		f.session->set_trigger(trigger);
		const auto expected = snapshot(trigger);
		{
			QSettings settings(f.filename(), QSettings::IniFormat);
			pv::save_trigger_settings(settings, f.session);
			settings.sync();
			BOOST_REQUIRE(settings.status() == QSettings::NoError);
		}
		f.session->set_trigger(nullptr);
		QSettings reopened(f.filename(), QSettings::IniFormat);
		BOOST_CHECK(pv::restore_trigger_settings(reopened, f.context, f.device, f.session));
		BOOST_CHECK_EQUAL(snapshot(f.session->trigger()), expected);
	}
}

BOOST_AUTO_TEST_CASE(stages_and_threshold_roundtrip)
{
	Fixture f;
	auto trigger = f.trigger(31);
	trigger->add_stage()->add_match(f.device->channels().at(32),
		sigrok::TriggerMatchType::RISING, 0.75f);
	f.session->set_trigger(trigger);
	const auto expected = snapshot(trigger);
	{
		QSettings settings(f.filename(), QSettings::IniFormat);
		pv::save_trigger_settings(settings, f.session);
		settings.sync();
	}
	f.session->set_trigger(nullptr);
	QSettings reopened(f.filename(), QSettings::IniFormat);
	BOOST_CHECK(pv::restore_trigger_settings(reopened, f.context, f.device, f.session));
	BOOST_CHECK_EQUAL(snapshot(f.session->trigger()), expected);
}

BOOST_AUTO_TEST_CASE(disabled_trigger_clears_existing)
{
	Fixture f;
	QSettings settings(f.filename(), QSettings::IniFormat);
	f.session->set_trigger(f.trigger());
	pv::save_trigger_settings(settings, f.session);
	f.session->set_trigger(nullptr);
	pv::save_trigger_settings(settings, f.session);
	f.session->set_trigger(f.trigger(7));
	BOOST_CHECK(pv::restore_trigger_settings(settings, f.context, f.device, f.session));
	BOOST_CHECK(!f.session->trigger());
	BOOST_CHECK(settings.value("trigger/stages").toList().empty());
}

BOOST_AUTO_TEST_CASE(legacy_setup_keeps_trigger)
{
	Fixture f;
	QSettings settings(f.filename(), QSettings::IniFormat);
	auto trigger = f.trigger();
	f.session->set_trigger(trigger);
	BOOST_CHECK(!pv::restore_trigger_settings(settings, f.context, f.device, f.session));
	BOOST_CHECK(f.session->trigger() == trigger);
}

BOOST_AUTO_TEST_CASE(invalid_setup_is_atomic)
{
	Fixture f;
	QSettings settings(f.filename(), QSettings::IniFormat);
	f.session->set_trigger(f.trigger());
	pv::save_trigger_settings(settings, f.session);
	const auto stages = settings.value("trigger/stages").toList();
	auto existing = f.trigger(7);
	f.session->set_trigger(existing);
	for (const auto &key : {"channel", "channel_type", "type", "value"}) {
		auto changed = stages;
		auto stage = changed[0].toMap();
		auto matches = stage["matches"].toList();
		auto item = matches[0].toMap();
		item[key] = QString("invalid");
		matches[0] = item;
		stage["matches"] = matches;
		changed[0] = stage;
		settings.setValue("trigger/stages", changed);
		BOOST_CHECK_THROW(pv::restore_trigger_settings(settings, f.context, f.device, f.session), std::exception);
		BOOST_CHECK(f.session->trigger() == existing);
	}
	settings.setValue("trigger/stages", QVariantList());
	BOOST_CHECK_THROW(pv::restore_trigger_settings(settings, f.context, f.device, f.session), std::exception);
	BOOST_CHECK(f.session->trigger() == existing);
}

BOOST_AUTO_TEST_CASE(unsupported_condition_keeps_existing)
{
	Fixture f;
	QSettings settings(f.filename(), QSettings::IniFormat);
	auto analog = f.context->create_trigger("unsupported");
	analog->add_stage()->add_match(f.device->channels().at(32), sigrok::TriggerMatchType::UNDER, 0.5f);
	f.session->set_trigger(analog);
	pv::save_trigger_settings(settings, f.session);
	auto existing = f.trigger();
	f.session->set_trigger(existing);
	BOOST_CHECK_THROW(pv::restore_trigger_settings(settings, f.context, f.device, f.session), std::exception);
	BOOST_CHECK(f.session->trigger() == existing);
}

BOOST_AUTO_TEST_CASE(restoring_does_not_enable_channels)
{
	Fixture f;
	QSettings settings(f.filename(), QSettings::IniFormat);
	f.session->set_trigger(f.trigger(31));
	pv::save_trigger_settings(settings, f.session);
	f.device->channels().at(31)->set_enabled(false);
	f.session->set_trigger(nullptr);
	BOOST_CHECK(pv::restore_trigger_settings(settings, f.context, f.device, f.session));
	BOOST_CHECK(!f.device->channels().at(31)->enabled());
	BOOST_REQUIRE(f.session->trigger());
	BOOST_CHECK_EQUAL(f.session->trigger()->stages().at(0)->matches().at(0)->channel()->index(), 31U);
}

BOOST_AUTO_TEST_SUITE_END()
