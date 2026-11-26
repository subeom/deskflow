/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClipboardReadHygieneTests.h"

#include "deskflow/ClipboardImageSize.h"
#include "deskflow/ClipboardReadCache.h"

#include <cstdint>
#include <string>

using namespace deskflow::clipboard;

namespace {

void be32(std::string &out, uint32_t value)
{
  for (int shift = 24; shift >= 0; shift -= 8)
    out.push_back(static_cast<char>((value >> shift) & 0xff));
}

// the signature and the IHDR chunk of a PNG: all that is needed to know its size
std::string png(uint32_t width, uint32_t height)
{
  std::string data("\x89PNG\r\n\x1a\n", 8);
  be32(data, 13);
  data += "IHDR";
  be32(data, width);
  be32(data, height);
  data += std::string("\x08\x06\x00\x00\x00", 5); // bit depth, colour type, compression, filter, interlace
  be32(data, 0);                                  // CRC, not checked
  data += "more of the file";
  return data;
}

void put16(std::string &out, uint16_t value, bool littleEndian)
{
  if (littleEndian) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>(value >> 8));
  } else {
    out.push_back(static_cast<char>(value >> 8));
    out.push_back(static_cast<char>(value & 0xff));
  }
}

void put32(std::string &out, uint32_t value, bool littleEndian)
{
  if (littleEndian) {
    for (int shift = 0; shift <= 24; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  } else {
    be32(out, value);
  }
}

// a TIFF with one directory that holds the width as SHORT and the height as LONG, in the order the file says
std::string tiff(uint32_t width, uint32_t height, bool littleEndian)
{
  std::string data = littleEndian ? "II" : "MM";
  put16(data, 42, littleEndian);
  put32(data, 8, littleEndian); // the directory follows the header
  put16(data, 3, littleEndian); // entries: a tag that is not of interest, the width and the height
  auto entry = [&](uint16_t tag, uint16_t type, uint32_t value) {
    put16(data, tag, littleEndian);
    put16(data, type, littleEndian);
    put32(data, 1, littleEndian);
    if (type == 3) { // SHORT: the value is in the first two bytes of the field
      put16(data, static_cast<uint16_t>(value), littleEndian);
      put16(data, 0, littleEndian);
    } else {
      put32(data, value, littleEndian);
    }
  };
  entry(254, 4, 0);
  entry(256, 3, width);
  entry(257, 4, height);
  put32(data, 0, littleEndian);
  return data;
}

} // namespace

void ClipboardReadHygieneTests::pngSize_readsTheHeader()
{
  const auto size = pngImageSize(png(2560, 1440));
  QVERIFY(size.has_value());
  QCOMPARE(size->width, uint32_t{2560});
  QCOMPARE(size->height, uint32_t{1440});
}

void ClipboardReadHygieneTests::pngSize_rejectsWhatIsNotAPng_data()
{
  QTest::addColumn<QByteArray>("data");
  QTest::newRow("empty") << QByteArray();
  QTest::newRow("shorter than the header") << QByteArray::fromStdString(png(1, 1).substr(0, 20));
  QTest::newRow("another signature") << QByteArray::fromStdString("GIF89a" + std::string(40, 'x'));
  auto notHeader = png(10, 10);
  notHeader.replace(12, 4, "IDAT");
  QTest::newRow("no IHDR first") << QByteArray::fromStdString(notHeader);
  QTest::newRow("a width of zero") << QByteArray::fromStdString(png(0, 10));
  QTest::newRow("a height of zero") << QByteArray::fromStdString(png(10, 0));
}

void ClipboardReadHygieneTests::pngSize_rejectsWhatIsNotAPng()
{
  QFETCH(QByteArray, data);
  QVERIFY(!pngImageSize(std::string_view(data.constData(), static_cast<size_t>(data.size()))).has_value());
}

void ClipboardReadHygieneTests::tiffSize_readsTheFirstDirectoryInEitherByteOrder_data()
{
  QTest::addColumn<bool>("littleEndian");
  QTest::newRow("little endian") << true;
  QTest::newRow("big endian") << false;
}

void ClipboardReadHygieneTests::tiffSize_readsTheFirstDirectoryInEitherByteOrder()
{
  QFETCH(bool, littleEndian);

  const auto size = tiffImageSize(tiff(1920, 1080, littleEndian));
  QVERIFY(size.has_value());
  QCOMPARE(size->width, uint32_t{1920});
  QCOMPARE(size->height, uint32_t{1080});
}

void ClipboardReadHygieneTests::tiffSize_rejectsWhatIsNotATiff_data()
{
  QTest::addColumn<QByteArray>("data");
  QTest::newRow("empty") << QByteArray();
  QTest::newRow("another signature") << QByteArray::fromStdString("BM" + std::string(40, 'x'));
  QTest::newRow("cut inside the directory") << QByteArray::fromStdString(tiff(10, 10, true).substr(0, 20));
  auto badVersion = tiff(10, 10, true);
  badVersion[2] = 43; // BigTIFF
  QTest::newRow("a version that is not 42") << QByteArray::fromStdString(badVersion);
  auto pointsOutside = tiff(10, 10, true);
  pointsOutside[4] = static_cast<char>(0xff);
  QTest::newRow("a directory beyond the data") << QByteArray::fromStdString(pointsOutside);
}

void ClipboardReadHygieneTests::tiffSize_rejectsWhatIsNotATiff()
{
  QFETCH(QByteArray, data);
  QVERIFY(!tiffImageSize(std::string_view(data.constData(), static_cast<size_t>(data.size()))).has_value());
}

void ClipboardReadHygieneTests::bitmapLowerBound_isFourBytesAPixelAndNeverWraps()
{
  QCOMPARE(bitmapLowerBound({1280, 720}), size_t{1280} * 720 * 4);
  QCOMPARE(bitmapLowerBound({1, 1}), size_t{4});
  // a corrupt header must not wrap around to a small number that lets a huge image through
  QCOMPARE(bitmapLowerBound({UINT32_MAX, UINT32_MAX}), SIZE_MAX);
}

void ClipboardReadHygieneTests::surelyOverLimit_countsWhatEveryClipboardMessageAdds_data()
{
  QTest::addColumn<qulonglong>("formatBytes");
  QTest::addColumn<qulonglong>("limitBytes");
  QTest::addColumn<bool>("over");

  // a marshalled clipboard is 4 bytes for the count, and 8 more for the format and the size of each format
  QTest::newRow("exactly the limit with the 12 bytes") << 1000ull << 1012ull << false;
  QTest::newRow("one byte more") << 1001ull << 1012ull << true;
  QTest::newRow("far below") << 10ull << 1012ull << false;
  QTest::newRow("a 3.7 MB image under a 3 MiB limit") << 3686400ull << 3145728ull << true;
  QTest::newRow("no limit") << 3686400ull << 0ull << false;
  QTest::newRow("nothing is known of the size") << 0ull << 1012ull << false;
}

void ClipboardReadHygieneTests::surelyOverLimit_countsWhatEveryClipboardMessageAdds()
{
  QFETCH(qulonglong, formatBytes);
  QFETCH(qulonglong, limitBytes);
  QFETCH(bool, over);

  QCOMPARE(surelyOverLimit(static_cast<size_t>(formatBytes), static_cast<size_t>(limitBytes)), over);
}

void ClipboardReadHygieneTests::cache_returnsWhatWasStoredUntilItIsInvalidated()
{
  ClipboardReadCache cache;
  QVERIFY(cache.find(0) == nullptr);

  cache.store(0, "hello");
  QVERIFY(cache.find(0) != nullptr);
  QCOMPARE(QString::fromStdString(*cache.find(0)), QStringLiteral("hello"));

  cache.invalidate();
  QVERIFY(cache.find(0) == nullptr);
}

void ClipboardReadHygieneTests::cache_keepsFormatsApart()
{
  ClipboardReadCache cache;
  cache.store(0, "text");
  cache.store(2, "image");

  QCOMPARE(QString::fromStdString(*cache.find(0)), QStringLiteral("text"));
  QCOMPARE(QString::fromStdString(*cache.find(2)), QStringLiteral("image"));
  QVERIFY(cache.find(1) == nullptr);

  cache.store(0, "newer");
  QCOMPARE(QString::fromStdString(*cache.find(0)), QStringLiteral("newer"));
}

void ClipboardReadHygieneTests::cache_doesNotKeepWhatIsTooLarge()
{
  ClipboardReadCache cache(100);
  cache.store(0, std::string(101, 'x'));
  QVERIFY(cache.find(0) == nullptr);

  cache.store(0, std::string(100, 'x'));
  QVERIFY(cache.find(0) != nullptr);
}

void ClipboardReadHygieneTests::cache_staysWithinItsBudget()
{
  ClipboardReadCache cache(100);
  cache.store(0, std::string(60, 'a'));
  cache.store(1, std::string(60, 'b')); // together over the budget: the older one goes

  QVERIFY(cache.find(1) != nullptr);
  QVERIFY(cache.find(0) == nullptr);
}

QTEST_MAIN(ClipboardReadHygieneTests)
