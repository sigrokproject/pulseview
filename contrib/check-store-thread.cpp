/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Standalone Linux export-worker regression. No physical drivers are scanned;
 * pthread_create is overridden only in this diagnostic process. */
#include <libsigrokcxx/libsigrokcxx.hpp>
#include <pv/application.hpp>
#include <pv/globalsettings.hpp>
#include <pv/devicemanager.hpp>
#include <pv/devices/inputfile.hpp>
#include <pv/devices/sessionfile.hpp>
#include <pv/session.hpp>
#include <pv/storesession.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdexcept>
#include <thread>

static std::atomic<bool> fail_next{false};
static std::atomic<unsigned> injected{0};
extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
 void* (*start)(void*), void* arg) noexcept
{
 using Function = int(*)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);
 static auto real = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "pthread_create"));
 if (!real) std::abort();
 if (fail_next.exchange(false)) { ++injected; return EAGAIN; }
 return real(thread, attr, start, arg);
}
static void require(bool condition, const char* text)
{
 if (!condition) throw std::runtime_error(text);
}
static void capture(pv::Session& session, Application& app)
{
 QString error;
 session.start_capture([&](QString e) { error = e; });
 const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(20);
 while (session.get_capture_state() != pv::Session::Stopped) {
  require(std::chrono::steady_clock::now() < end, "File replay timeout");
  app.processEvents();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 session.stop_capture();
 require(error.isEmpty(), "File replay failed");
}
static std::vector<uint8_t> bytes(pv::Session& session)
{
 auto segment = session.signalbases().at(0)->logic_data()->logic_segments().at(0);
 std::vector<uint8_t> data(segment->get_sample_count() * segment->unit_size());
 segment->get_samples(0, segment->get_sample_count(), data.data());
 return data;
}
int main(int argc, char** argv) try
{
 require(argc == 2, "Output argument required");
 QDir out(QString::fromLocal8Bit(argv[1]));
 require(!out.exists() && QDir().mkpath(out.path()), "Existing output directory");
 QSettings::setDefaultFormat(QSettings::IniFormat);
 QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, out.filePath("settings"));
 QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, out.filePath("settings"));
 Application app(argc, argv);
 pv::GlobalSettings settings;
 settings.save_internal_defaults(); settings.set_defaults_where_needed();
 // This data-export fixture has no view whose setup could be saved.
 settings.setValue(pv::GlobalSettings::Key_General_SaveWithSetup, false);
 qRegisterMetaType<uint64_t>("uint64_t");
 qRegisterMetaType<pv::util::Timestamp>("util::Timestamp");
 qRegisterMetaType<SharedPtrToSegment>("SharedPtrToSegment");
 auto context = sigrok::Context::create();
 pv::Session::sr_context = context;
 unsigned failures = 0, cases = 0;
 {
  pv::DeviceManager manager(context, "", false);
  for (uint64_t count : {UINT64_C(2048), UINT64_C(2621441)}) {
   std::vector<uint8_t> expected(count * 4);
   for (size_t i = 0; i < expected.size(); ++i) expected[i] = uint8_t(i * 37 + (i >> 5));
   const auto raw = out.filePath(QString::number(count) + ".bin");
   QFile file(raw);
   require(file.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
    file.write(reinterpret_cast<const char*>(expected.data()), expected.size()) == qint64(expected.size()), "Fixture write failed");
   file.close();
   pv::Session session(manager, "export worker fixture");
   session.set_device(std::make_shared<pv::devices::InputFile>(context,
    raw.toStdString(), context->input_formats().at("binary"),
    std::map<std::string, Glib::VariantBase>{{"numchannels", Glib::Variant<gint32>::create(32)},
    {"samplerate", Glib::Variant<guint64>::create(1000000)}}));
   capture(session, app);
   require(bytes(session) == expected, "Input bytes differ");
   for (unsigned kind = 0; kind < 2; ++kind) {
    const auto format = kind ? "srzip" : "binary";
    const auto extension = kind ? ".sr" : ".bin";
    const QString label = QString::number(count) + "-" + QString::number(kind);
    unsigned success = 0;
    bool started = false, escaped = false;
    QString error;
    const auto before = injected.load();
    {
     pv::StoreSession store(out.filePath(label + "-fault" + extension).toStdString(),
      context->output_formats().at(format), {}, {0, 0}, session);
     QObject::connect(&store, &pv::StoreSession::store_successful, [&] { ++success; });
     fail_next = true;
     try { started = store.start(); }
     catch (const std::exception& e) { escaped = true; error = QString::fromUtf8(e.what()); }
     if (started) store.wait();
     fail_next = false;
     if (error.isEmpty()) error = store.error();
     const bool good = injected == before + 1 && !escaped && !started && !error.isEmpty() && !success;
     if (!good) ++failures;
     std::printf("count=%llu kind=%u injected=%u started=%d escaped=%d successes=%u error=%s contract=%s\n",
      (unsigned long long)count, kind, injected.load() - before, started, escaped,
      success, error.toUtf8().constData(), good ? "PASS" : "FAIL");
     ++cases;
    }
    require(bytes(session) == expected, "Failed export changed source bytes");
    const auto path = out.filePath(label + "-control" + extension);
    success = 0;
    {
     pv::StoreSession store(path.toStdString(), context->output_formats().at(format), {}, {0, 0}, session);
     QObject::connect(&store, &pv::StoreSession::store_successful, [&] { ++success; });
     require(store.start(), "Control export failed to start"); store.wait();
     require(store.error().isEmpty() && success == 1, "Control export failed");
    }
    if (!kind) {
     QFile f(path); require(f.open(QIODevice::ReadOnly) &&
      f.readAll() == QByteArray(reinterpret_cast<const char*>(expected.data()), expected.size()), "Binary export differs");
    } else {
     pv::Session replay(manager, "export replay");
     replay.set_device(std::make_shared<pv::devices::SessionFile>(context, path.toStdString()));
     capture(replay, app); require(bytes(replay) == expected, "Archive replay differs");
    }
    require(bytes(session) == expected, "Control changed source bytes"); ++cases;
   }
  }
 }
 pv::Session::sr_context.reset();
 std::printf("%u export fault/control cases, %u failures, no physical scans\n", cases, failures);
 return failures ? 1 : 0;
} catch (const std::exception& e) {
 std::fprintf(stderr, "export-probe: %s\n", e.what()); return 2;
}
