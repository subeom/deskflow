/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorReportTests.h"

#include "deskflow/MonitorReport.h"
#include "deskflow/ProtocolTypes.h"
#include "io/IStream.h"

#include <QByteArray>
#include <QTest>

#include <algorithm>
#include <cstring>

using namespace deskflow::monitors;
using deskflow::server::MonitorRect;

namespace {

// the bytes that are left to read, as one piece
class BufferStream : public deskflow::IStream
{
public:
  explicit BufferStream(QByteArray bytes) : m_bytes(std::move(bytes))
  {
  }

  void close() override
  {
    m_bytes.clear();
  }

  uint32_t read(void *buffer, uint32_t n) override
  {
    const auto count = std::min<qsizetype>(n, m_bytes.size());
    if (buffer != nullptr && count > 0) {
      std::memcpy(buffer, m_bytes.constData(), static_cast<size_t>(count));
    }
    m_bytes.remove(0, count);
    return static_cast<uint32_t>(count);
  }

  void write(const void *, uint32_t) override
  {
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<BufferStream *>(this);
  }
  bool isReady() const override
  {
    return !m_bytes.isEmpty();
  }
  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(m_bytes.size());
  }

  QByteArray left() const
  {
    return m_bytes;
  }

private:
  QByteArray m_bytes;
};

QByteArray bytes(const std::vector<uint8_t> &v)
{
  return QByteArray(reinterpret_cast<const char *>(v.data()), static_cast<qsizetype>(v.size()));
}

// what follows the code of a message
QByteArray afterTheCode(const std::vector<uint8_t> &message)
{
  return bytes(message).mid(4);
}

MonitorInfo info(int x, int y, int w, int h, uint16_t scale = 100, bool primary = false)
{
  return {{x, y, w, h}, scale, primary};
}

QByteArray hex(const char *text)
{
  return QByteArray::fromHex(text);
}

} // namespace

void MonitorReportTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Debug);
}

void MonitorReportTests::encode_oneMonitor_goldenBytes()
{
  MonitorReport report;
  report.generation = 1;
  report.monitors = {{0, 0, 1920, 1080, 150, true}};

  // the example of the protocol reference
  QCOMPARE(bytes(encodeReport(report)), hex("444d4f4e 01 0001 01 0000 0000 0780 0438 0096 01"));
  QCOMPARE(std::memcmp(kMsgDMonitors, "DMON", 4), 0);
}

void MonitorReportTests::encode_negativeOriginAndTwoMonitors_goldenBytes()
{
  MonitorReport report;
  report.generation = 0x0102;
  report.monitors = {{-1920, -1080, 1920, 1080, 100, false}, {0, -1080, 2560, 1440, 200, true}};

  QCOMPARE(
      bytes(encodeReport(report)), hex("444d4f4e 01 0102 02 f880 fbc8 0780 0438 0064 00 0000 fbc8 0a00 05a0 00c8 01")
  );
}

void MonitorReportTests::encode_noMonitors_isTheHeaderOnly()
{
  QCOMPARE(bytes(encodeReport({7, {}})), hex("444d4f4e 01 0007 00"));
}

void MonitorReportTests::encode_neverWritesMoreThanTheCountCanSay()
{
  MonitorReport report;
  report.generation = 1;
  report.monitors.assign(300, MonitorEntry{0, 0, 1, 1, 100, false});
  const auto message = encodeReport(report);

  QCOMPARE(static_cast<int>(message[7]), 255);
  QCOMPARE(message.size(), static_cast<size_t>(8 + 255 * kRecordSize));
}

void MonitorReportTests::read_roundTrips()
{
  MonitorReport report;
  report.generation = 65535;
  report.monitors = {{-32768, -1, 32767, 1080, 125, true}, {5, 6, 7, 8, 0, false}};
  BufferStream stream(afterTheCode(encodeReport(report)));

  const auto read = readReport(&stream);
  QVERIFY(read.has_value());
  QCOMPARE(read->version, kReportVersion);
  QCOMPARE(read->generation, static_cast<uint16_t>(65535));
  QCOMPARE(read->count, static_cast<uint8_t>(2));
  QCOMPARE(read->monitors.size(), static_cast<size_t>(2));
  QVERIFY(read->monitors == report.monitors);
  QVERIFY(stream.left().isEmpty());
}

void MonitorReportTests::read_readsTheWholeMessageWhateverItHolds_data()
{
  QTest::addColumn<int>("count");
  QTest::addColumn<int>("fill");
  QTest::addColumn<int>("version");

  QTest::newRow("no monitors") << 0 << 0x00 << 1;
  QTest::newRow("sixteen") << 16 << 0x11 << 1;
  QTest::newRow("seventeen") << 17 << 0x22 << 1;
  QTest::newRow("the most the count can say") << 255 << 0xff << 1;
  QTest::newRow("all zero: no size") << 3 << 0x00 << 1;
  QTest::newRow("all ones: negative sizes") << 3 << 0xff << 1;
  QTest::newRow("a version that does not exist") << 4 << 0x7f << 9;
}

// what is after the message is the next message, whatever the message held: that is what keeps the stream
void MonitorReportTests::read_readsTheWholeMessageWhateverItHolds()
{
  QFETCH(int, count);
  QFETCH(int, fill);
  QFETCH(int, version);

  QByteArray message;
  message.append(static_cast<char>(version));
  message.append(char(0x12));
  message.append(char(0x34));
  message.append(static_cast<char>(count));
  message.append(QByteArray(count * static_cast<int>(kRecordSize), static_cast<char>(fill)));
  message.append("CNOP");
  BufferStream stream(message);

  const auto read = readReport(&stream);
  QVERIFY(read.has_value());
  QCOMPARE(static_cast<int>(read->count), count);
  QCOMPARE(read->monitors.size(), static_cast<size_t>(count));
  QCOMPARE(static_cast<int>(read->version), version);
  QCOMPARE(read->generation, static_cast<uint16_t>(0x1234));
  QCOMPARE(stream.left(), QByteArray("CNOP"));
}

void MonitorReportTests::read_truncated_isEmptyAndDoesNotThrow_data()
{
  QTest::addColumn<QByteArray>("message");

  QTest::newRow("nothing") << QByteArray();
  QTest::newRow("a part of the header") << hex("01 00");
  QTest::newRow("the header only, but a count") << hex("01 0001 02");
  QTest::newRow("a record cut short") << hex("01 0001 01 0000 0000 0780");
  QTest::newRow("one record of two") << hex("01 0001 02 0000 0000 0780 0438 0064 00");
  QTest::newRow("a count of 255 and nothing more") << hex("01 0001 ff");
}

void MonitorReportTests::read_truncated_isEmptyAndDoesNotThrow()
{
  QFETCH(QByteArray, message);
  BufferStream stream(message);

  const auto read = readReport(&stream);
  QVERIFY(!read.has_value());
  // it read what there was and no more
  QVERIFY(stream.left().isEmpty());
}

void MonitorReportTests::prepare_tilesTheShape_data()
{
  QTest::addColumn<QList<int>>("monitors");
  QTest::addColumn<QList<int>>("shape");

  QTest::newRow("one monitor") << QList<int>{0, 0, 1920, 1080} << QList<int>{0, 0, 1920, 1080};
  QTest::newRow("side by side") << QList<int>{0, 0, 2560, 1440, 2560, 0, 1920, 1080} << QList<int>{0, 0, 4480, 1440};
  QTest::newRow("a monitor on the left of the origin")
      << QList<int>{-1920, 0, 1920, 1080, 0, 0, 2560, 1440} << QList<int>{-1920, 0, 4480, 1440};
  QTest::newRow("above, negative y") << QList<int>{0, -1080, 1920, 1080, 0, 0, 1920, 1080}
                                     << QList<int>{0, -1080, 1920, 2160};
  QTest::newRow("the largest a shape can say")
      << QList<int>{-32768, -32768, 32767, 32767, -1, -32768, 32767, 32767} << QList<int>{-32768, -32768, 65534, 32767};
}

void MonitorReportTests::prepare_tilesTheShape()
{
  QFETCH(QList<int>, monitors);
  QFETCH(QList<int>, shape);

  std::vector<MonitorInfo> list;
  for (int i = 0; i + 3 < monitors.size(); i += 4) {
    list.push_back(info(monitors[i], monitors[i + 1], monitors[i + 2], monitors[i + 3]));
  }
  const auto prepared = prepareMonitors(list, {shape[0], shape[1], shape[2], shape[3]});

  QVERIFY2(prepared.ok(), prepared.problem.c_str());
  QCOMPARE(prepared.monitors.size(), list.size());
  QCOMPARE(prepared.duplicatesRemoved, static_cast<size_t>(0));
  for (size_t i = 0; i < list.size(); ++i) {
    QVERIFY(prepared.monitors[i].rect() == list[i].rect);
  }
}

void MonitorReportTests::prepare_cannotReport_data()
{
  QTest::addColumn<QList<int>>("monitors");
  QTest::addColumn<QList<int>>("shape");
  QTest::addColumn<QString>("problem");

  QTest::newRow("no monitors") << QList<int>{} << QList<int>{0, 0, 1920, 1080} << "no monitors";
  QTest::newRow("they cover less than the shape")
      << QList<int>{0, 0, 1920, 1080} << QList<int>{0, 0, 4480, 1440} << "screen";
  QTest::newRow("they cover more than the shape")
      << QList<int>{0, 0, 1920, 1080, 1920, 0, 1920, 1080} << QList<int>{0, 0, 1920, 1080} << "screen";
  QTest::newRow("another origin") << QList<int>{0, 0, 1920, 1080} << QList<int>{1, 0, 1920, 1080} << "screen";
  QTest::newRow("overlap") << QList<int>{0, 0, 1920, 1080, 1000, 0, 1920, 1080} << QList<int>{0, 0, 2920, 1080}
                           << "overlap";
  QTest::newRow("no width") << QList<int>{0, 0, 0, 1080} << QList<int>{0, 0, 0, 1080} << "size";
  QTest::newRow("negative height") << QList<int>{0, 0, 1920, -5} << QList<int>{0, 0, 1920, -5} << "size";
  QTest::newRow("a position that is not 16 bits")
      << QList<int>{40000, 0, 1920, 1080} << QList<int>{40000, 0, 1920, 1080} << "16 bits";
  QTest::newRow("a right edge that is not 16 bits")
      << QList<int>{0, 0, 32768, 1080} << QList<int>{0, 0, 32768, 1080} << "16 bits";
  QTest::newRow("a bottom edge that is not 16 bits")
      << QList<int>{0, 30000, 1920, 3000} << QList<int>{0, 30000, 1920, 3000} << "16 bits";
}

void MonitorReportTests::prepare_cannotReport()
{
  QFETCH(QList<int>, monitors);
  QFETCH(QList<int>, shape);
  QFETCH(QString, problem);

  std::vector<MonitorInfo> list;
  for (int i = 0; i + 3 < monitors.size(); i += 4) {
    list.push_back(info(monitors[i], monitors[i + 1], monitors[i + 2], monitors[i + 3]));
  }
  const auto prepared = prepareMonitors(list, {shape[0], shape[1], shape[2], shape[3]});

  QVERIFY(!prepared.ok());
  QVERIFY2(QString::fromStdString(prepared.problem).contains(problem), prepared.problem.c_str());
  QVERIFY(prepared.monitors.empty());
}

void MonitorReportTests::prepare_mirroredMonitorsAreOne()
{
  // a mirrored display has the rectangle of the one that it mirrors; the primary flag is not lost
  const auto prepared = prepareMonitors(
      {info(0, 0, 1920, 1080, 100, false), info(0, 0, 1920, 1080, 150, true), info(1920, 0, 1280, 1024, 100, false)},
      {0, 0, 3200, 1080}
  );

  QVERIFY2(prepared.ok(), prepared.problem.c_str());
  QCOMPARE(prepared.duplicatesRemoved, static_cast<size_t>(1));
  QCOMPARE(prepared.monitors.size(), static_cast<size_t>(2));
  QVERIFY(prepared.monitors[0].rect() == (MonitorRect{0, 0, 1920, 1080}));
  QVERIFY(prepared.monitors[0].primary);
  QCOMPARE(prepared.monitors[0].scalePercent, static_cast<uint16_t>(100));
}

void MonitorReportTests::prepare_keepsScaleAndPrimary()
{
  const auto prepared =
      prepareMonitors({info(0, 0, 1920, 1080, 150, true), info(1920, 0, 1920, 1080, 100, false)}, {0, 0, 3840, 1080});

  QVERIFY2(prepared.ok(), prepared.problem.c_str());
  QCOMPARE(prepared.monitors[0].scalePercent, static_cast<uint16_t>(150));
  QVERIFY(prepared.monitors[0].primary);
  QCOMPARE(prepared.monitors[1].scalePercent, static_cast<uint16_t>(100));
  QVERIFY(!prepared.monitors[1].primary);
}

void MonitorReportTests::prepare_sixteenMonitorsAreTheMost()
{
  const auto row = [](int count) {
    std::vector<MonitorInfo> list;
    for (int i = 0; i < count; ++i) {
      list.push_back(info(i * 100, 0, 100, 100));
    }
    return list;
  };

  QVERIFY(prepareMonitors(row(16), {0, 0, 1600, 100}).ok());
  const auto tooMany = prepareMonitors(row(17), {0, 0, 1700, 100});
  QVERIFY(!tooMany.ok());
  QVERIFY2(QString::fromStdString(tooMany.problem).contains(QStringLiteral("16")), tooMany.problem.c_str());
}

QTEST_MAIN(MonitorReportTests)
