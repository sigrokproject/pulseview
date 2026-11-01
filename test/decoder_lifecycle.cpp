/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrokdecode/libsigrokdecode.h>

#include <pv/application.hpp>
#include <pv/data/decodesignal.hpp>
#include <pv/data/decode/decoder.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/devicemanager.hpp>
#include <pv/devices/inputfile.hpp>
#include <pv/globalsettings.hpp>
#include <pv/session.hpp>

#include <boost/test/unit_test.hpp>
#include <QFile>
#include <QTemporaryDir>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
std::mutex gate_mutex;
std::condition_variable gate_condition;
bool entered = false, released = false;
std::atomic<bool> armed{false}, invalidated{false};
std::atomic<unsigned> pending{0}, early_destroy{0};

void require(bool ok, const char *message)
{
	if (!ok)
		throw std::runtime_error(message);
}

struct Runtime {
	QTemporaryDir settings_directory;
	std::unique_ptr<Application> application;

	Runtime()
	{
		static int argc = 1;
		static char name[] = "pulseview-decoder-test";
		static char *argv[] = {name, nullptr};
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			settings_directory.path());
		QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
			settings_directory.path());
		application.reset(new Application(argc, argv));
		qRegisterMetaType<uint64_t>("uint64_t");
		qRegisterMetaType<pv::util::Timestamp>("util::Timestamp");
		qRegisterMetaType<SharedPtrToSegment>("SharedPtrToSegment");
		pv::GlobalSettings settings;
		settings.save_internal_defaults();
		settings.set_defaults_where_needed();
		require(srd_init(nullptr) == SRD_OK &&
			srd_decoder_load("uart") == SRD_OK, "Cannot initialize UART");
	}

	~Runtime() { srd_exit(); }
};

// Always release the held call, including when a check throws.
struct Release {
	std::thread thread;
	Release() : thread([] {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::lock_guard<std::mutex> lock(gate_mutex);
		released = true;
		gate_condition.notify_all();
	}) {}
	~Release() { thread.join(); }
};
} // namespace

extern "C" int __real_srd_session_send(srd_session *, uint64_t, uint64_t,
	const uint8_t *, uint64_t, uint64_t);
extern "C" int __real_srd_session_destroy(srd_session *);

extern "C" int __wrap_srd_session_send(srd_session *session, uint64_t start,
	uint64_t end, const uint8_t *data, uint64_t length, uint64_t unit)
{
	if (!armed.exchange(false))
		return __real_srd_session_send(session, start, end, data, length, unit);

	++pending;
	{
		std::unique_lock<std::mutex> lock(gate_mutex);
		entered = true;
		gate_condition.notify_all();
		if (!gate_condition.wait_for(lock, std::chrono::seconds(5), [] { return released; })) {
			--pending;
			return SRD_ERR;
		}
	}
	// The negative control must record the violation without using freed memory.
	const int result = invalidated ? SRD_ERR :
		__real_srd_session_send(session, start, end, data, length, unit);
	--pending;
	return result;
}

extern "C" int __wrap_srd_session_destroy(srd_session *session)
{
	if (pending) {
		++early_destroy;
		invalidated = true;
	}
	return __real_srd_session_destroy(session);
}

BOOST_AUTO_TEST_SUITE(DecoderLifecycleTest)

BOOST_AUTO_TEST_CASE(shutdown_joins_pending_send_and_initial_progress_is_zero)
{
	static Runtime runtime;
	QTemporaryDir directory;
	QFile file(directory.filePath("uart.bin"));
	BOOST_REQUIRE(file.open(QIODevice::WriteOnly));
	QByteArray bytes(2048, 1);
	for (unsigned bit = 0; bit < 10; ++bit)
		for (unsigned sample = 0; sample < 100; ++sample)
			bytes[100 + bit * 100 + sample] =
				(bit == 9 || (bit && ((0x55 >> (bit - 1)) & 1))) ? 1 : 0;
	BOOST_REQUIRE_EQUAL(file.write(bytes), bytes.size());
	file.close();

	auto context = sigrok::Context::create();
	pv::Session::sr_context = context;
	int64_t initial_progress = -1;
	{
		pv::DeviceManager manager(context, "", false);
		pv::Session session(manager, "decoder lifetime");
		session.set_device(std::make_shared<pv::devices::InputFile>(context,
			file.fileName().toStdString(), context->input_formats().at("binary"),
			std::map<std::string, Glib::VariantBase>{
				{"numchannels", Glib::Variant<gint32>::create(8)},
				{"samplerate", Glib::Variant<guint64>::create(1000000)}}));
		QString error;
		session.start_capture([&](const QString &message) { error = message; });
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (session.get_capture_state() != pv::Session::Stopped) {
			require(std::chrono::steady_clock::now() < deadline, "Input deadline");
			runtime.application->processEvents();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		session.stop_capture();
		runtime.application->processEvents();
		require(error.isEmpty(), "Input failed");

		armed = true;
		auto decoded = session.add_decode_signal();
		decoded->stack_decoder(srd_decoder_get_by_id("uart"), false);
		decoded->decoder_stack().front()->set_option("baudrate", g_variant_new_int64(10000));
		for (const auto &channel : decoded->get_channels())
			if (std::strcmp(channel.pdch_->id, "rx") == 0)
				decoded->assign_signal(channel.id, session.signalbases().at(0));
		{
			std::unique_lock<std::mutex> lock(gate_mutex);
			require(gate_condition.wait_for(lock, std::chrono::seconds(5), [] { return entered; }),
				"Send did not reach the SDK boundary");
		}
		initial_progress = decoded->get_decoded_sample_count(0, false);
		decoded->pause_decode();
		Release release;
		decoded->reset_decode(true);
	}
	pv::Session::sr_context.reset();
	BOOST_CHECK_EQUAL(pending.load(), 0);
	BOOST_CHECK_EQUAL(early_destroy.load(), 0);
	BOOST_CHECK_EQUAL(initial_progress, 0);
}

BOOST_AUTO_TEST_SUITE_END()
