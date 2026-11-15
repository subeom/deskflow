/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClipboardSharePolicyTests.h"

#include "deskflow/Clipboard.h"
#include "deskflow/ClipboardSharePolicy.h"

#include <array>
#include <optional>
#include <string>

namespace {

using Policy = ClipboardSharePolicy;
using Format = IClipboard::Format;

constexpr auto kLimit = ClipboardSharePolicy::kTextOnlyLimitBytes;
constexpr auto kMaxText = ClipboardSharePolicy::kTextOnlyMaxTextBytes;
constexpr int kNoText = -1;

std::string bytes(size_t count)
{
  return std::string(count, 'a');
}

// What a clipboard holds: the text with this many bytes (or none), and whether it has HTML and a bitmap besides
void fill(Clipboard &clipboard, int textBytes, bool html, bool bitmap)
{
  clipboard.open(0);
  clipboard.empty();
  if (textBytes != kNoText)
    clipboard.add(Format::Text, bytes(static_cast<size_t>(textBytes)));
  if (html)
    clipboard.add(Format::HTML, "<b>bold</b>");
  if (bitmap)
    clipboard.add(Format::Bitmap, std::string(2048, 'x'));
  clipboard.close();
}

Policy::Content contentOf(int textBytes, bool html, bool bitmap)
{
  Policy::Content content;
  if (textBytes != kNoText)
    content.text = static_cast<size_t>(textBytes);
  content.html = html;
  content.bitmap = bitmap;
  return content;
}

// What the marshalled clipboard of nothing looks like: a format count of 0
const std::string kEmptyMarshalled(4, '\0');

// A clipboard that counts what is read from it
class SpyClipboard : public IClipboard
{
public:
  bool empty() override
  {
    return true;
  }
  void add(Format, const std::string &) override
  {
  }
  bool open(Time) const override
  {
    return true;
  }
  void close() const override
  {
  }
  Time getTime() const override
  {
    return 0;
  }
  bool has(Format format) const override
  {
    ++m_has[static_cast<size_t>(format)];
    return true;
  }
  std::string get(Format format) const override
  {
    ++m_get[static_cast<size_t>(format)];
    return "data of " + std::to_string(static_cast<int>(format));
  }

  int hasCalls(Format format) const
  {
    return m_has[static_cast<size_t>(format)];
  }
  int getCalls(Format format) const
  {
    return m_get[static_cast<size_t>(format)];
  }

private:
  mutable std::array<int, static_cast<size_t>(Format::TotalFormats)> m_has{};
  mutable std::array<int, static_cast<size_t>(Format::TotalFormats)> m_get{};
};

} // namespace

void ClipboardSharePolicyTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Verbose);
}

// Every peer compares the size of the marshalled clipboard with the size option, so that is what the limit is made of
void ClipboardSharePolicyTests::limit_isTenKiBOfMarshalledClipboard()
{
  QCOMPARE(kLimit, static_cast<size_t>(10 * 1024));
  QCOMPARE(kMaxText, static_cast<size_t>(10228));
  QCOMPARE(kMaxText + Policy::kMarshalledTextOverheadBytes, kLimit);

  // and the overhead is what marshall() really adds around the text
  for (const size_t textBytes : {static_cast<size_t>(0), static_cast<size_t>(1), static_cast<size_t>(777)}) {
    Clipboard clipboard;
    fill(clipboard, static_cast<int>(textBytes), false, false);
    QCOMPARE(clipboard.marshall().size(), textBytes + Policy::kMarshalledTextOverheadBytes);
  }
}

void ClipboardSharePolicyTests::off_sharesEverythingAsIs_data()
{
  QTest::addColumn<int>("textBytes");
  QTest::addColumn<bool>("html");
  QTest::addColumn<bool>("bitmap");

  for (const int textBytes : {kNoText, 0, 1, static_cast<int>(kMaxText), static_cast<int>(kLimit), 1024 * 1024}) {
    for (const bool html : {false, true}) {
      for (const bool bitmap : {false, true}) {
        QTest::addRow("text %d B, html %d, bitmap %d", textBytes, static_cast<int>(html), static_cast<int>(bitmap))
            << textBytes << html << bitmap;
      }
    }
  }
}

// Off is what it always was: the clipboard is shared as a whole and the decision is left to the size limit
void ClipboardSharePolicyTests::off_sharesEverythingAsIs()
{
  QFETCH(int, textBytes);
  QFETCH(bool, html);
  QFETCH(bool, bitmap);

  const Policy policy(false);
  QVERIFY(!policy.textOnly());
  QVERIFY(policy.decide(contentOf(textBytes, html, bitmap)) == Policy::Decision{});

  Clipboard clipboard;
  fill(clipboard, textBytes, html, bitmap);
  const auto before = clipboard.marshall();
  QVERIFY(policy.apply(clipboard) == Policy::Decision{});
  QCOMPARE(clipboard.marshall(), before);
}

void ClipboardSharePolicyTests::on_decide_data()
{
  QTest::addColumn<int>("textBytes");
  QTest::addColumn<bool>("html");
  QTest::addColumn<bool>("bitmap");
  QTest::addColumn<int>("action");
  QTest::addColumn<int>("reason");

  const auto row = [](const char *name, int textBytes, bool html, bool bitmap, Policy::Action action,
                      Policy::Reason reason) {
    QTest::newRow(name) << textBytes << html << bitmap << static_cast<int>(action) << static_cast<int>(reason);
  };
  using enum Policy::Action;
  using enum Policy::Reason;

  row("text 0 B", 0, false, false, ShareText, None);
  row("text 1 B", 1, false, false, ShareText, None);
  row("marshalled clipboard of exactly 10240 B (text 10228 B)", static_cast<int>(kMaxText), false, false, ShareText,
      None);
  row("marshalled clipboard of 10241 B (text 10229 B)", static_cast<int>(kMaxText) + 1, false, false, Drop,
      TextTooLarge);
  row("text of exactly 10240 B", static_cast<int>(kLimit), false, false, Drop, TextTooLarge);
  row("text of 10241 B", static_cast<int>(kLimit) + 1, false, false, Drop, TextTooLarge);
  row("text of 1 MiB", 1024 * 1024, false, false, Drop, TextTooLarge);
  row("text and HTML", 5, true, false, ShareText, None);
  row("text and bitmap", 5, false, true, ShareText, None);
  row("text, HTML and bitmap", 5, true, true, ShareText, None);
  row("text that fits, with a large HTML and bitmap", static_cast<int>(kMaxText), true, true, ShareText, None);
  row("long text and HTML", static_cast<int>(kLimit), true, false, Drop, TextTooLarge);
  row("bitmap only", kNoText, false, true, Drop, NoText);
  row("HTML only", kNoText, true, false, Drop, NoText);
  row("HTML and bitmap", kNoText, true, true, Drop, NoText);
  row("empty clipboard", kNoText, false, false, Drop, NoText);
}

void ClipboardSharePolicyTests::on_decide()
{
  QFETCH(int, textBytes);
  QFETCH(bool, html);
  QFETCH(bool, bitmap);
  QFETCH(int, action);
  QFETCH(int, reason);

  const Policy policy(true);
  QVERIFY(policy.textOnly());
  const auto decision = policy.decide(contentOf(textBytes, html, bitmap));
  QCOMPARE(static_cast<int>(decision.action), action);
  QCOMPARE(static_cast<int>(decision.reason), reason);
  QCOMPARE(decision.textBytes, textBytes == kNoText ? static_cast<size_t>(0) : static_cast<size_t>(textBytes));
}

void ClipboardSharePolicyTests::on_decide_countsBytesOfUtf8_data()
{
  QTest::addColumn<QByteArray>("text");
  QTest::addColumn<int>("action");

  const auto repeated = [](const QString &character, int count) {
    QString text;
    for (int i = 0; i < count; ++i)
      text += character;
    return text.toUtf8();
  };
  const auto row = [](const char *name, const QByteArray &text, Policy::Action action) {
    QTest::newRow(name) << text << static_cast<int>(action);
  };
  const QString hangul(QChar(0xD55C));                    // 3 bytes
  const QString emoji = QString::fromUcs4(U"\U0001F600"); // 4 bytes
  using enum Policy::Action;

  // 3409 characters of 3 bytes are 10227 B, 3410 are 10230 B: more than 10228 B although far fewer characters
  row("3409 Hangul syllables, 10227 B", repeated(hangul, 3409), ShareText);
  row("3410 Hangul syllables, 10230 B", repeated(hangul, 3410), Drop);
  // the last character straddles the limit: it is whole or it is not shared, it is never cut
  row("10225 ASCII and one Hangul syllable, 10228 B", QByteArray(10225, 'a') + hangul.toUtf8(), ShareText);
  row("10226 ASCII and one Hangul syllable, 10229 B", QByteArray(10226, 'a') + hangul.toUtf8(), Drop);
  row("2557 emoji, 10228 B", repeated(emoji, 2557), ShareText);
  row("2558 emoji, 10232 B", repeated(emoji, 2558), Drop);
}

// The limit counts the bytes that are sent, not the characters the user sees
void ClipboardSharePolicyTests::on_decide_countsBytesOfUtf8()
{
  QFETCH(QByteArray, text);
  QFETCH(int, action);

  const Policy policy(true);
  Clipboard clipboard;
  clipboard.open(0);
  clipboard.empty();
  clipboard.add(Format::Text, text.toStdString());
  clipboard.close();

  const auto decision = policy.apply(clipboard);
  QCOMPARE(static_cast<int>(decision.action), action);
  QCOMPARE(decision.textBytes, static_cast<size_t>(text.size()));

  QVERIFY(clipboard.open(0));
  if (action == static_cast<int>(Policy::Action::ShareText)) {
    // untouched, down to the last byte of the last character
    QCOMPARE(clipboard.get(Format::Text), text.toStdString());
    QVERIFY(clipboard.marshall().size() <= kLimit);
  } else {
    // not cut short either: nothing is left
    QVERIFY(!clipboard.has(Format::Text));
  }
  clipboard.close();
}

void ClipboardSharePolicyTests::on_apply_keepsOnlyTheText()
{
  const Policy policy(true);
  Clipboard clipboard;
  fill(clipboard, 42, true, true);

  Clipboard textOnly;
  fill(textOnly, 42, false, false);

  const auto decision = policy.apply(clipboard);
  QCOMPARE(static_cast<int>(decision.action), static_cast<int>(Policy::Action::ShareText));
  QCOMPARE(decision.textBytes, static_cast<size_t>(42));
  QCOMPARE(clipboard.marshall(), textOnly.marshall());

  QVERIFY(clipboard.open(0));
  QVERIFY(clipboard.has(Format::Text));
  QVERIFY(!clipboard.has(Format::HTML));
  QVERIFY(!clipboard.has(Format::Bitmap));
  clipboard.close();
}

void ClipboardSharePolicyTests::on_apply_emptiesWhatIsNotShared_data()
{
  QTest::addColumn<int>("textBytes");
  QTest::addColumn<bool>("html");
  QTest::addColumn<bool>("bitmap");

  QTest::newRow("long text") << static_cast<int>(kLimit) << false << false;
  QTest::newRow("long text with HTML and bitmap") << 1024 * 1024 << true << true;
  QTest::newRow("bitmap only") << kNoText << false << true;
  QTest::newRow("HTML only") << kNoText << true << false;
  QTest::newRow("empty clipboard") << kNoText << false << false;
}

void ClipboardSharePolicyTests::on_apply_emptiesWhatIsNotShared()
{
  QFETCH(int, textBytes);
  QFETCH(bool, html);
  QFETCH(bool, bitmap);

  const Policy policy(true);
  Clipboard clipboard;
  fill(clipboard, textBytes, html, bitmap);

  const auto decision = policy.apply(clipboard);
  QCOMPARE(static_cast<int>(decision.action), static_cast<int>(Policy::Action::Drop));
  QCOMPARE(clipboard.marshall(), kEmptyMarshalled);
}

void ClipboardSharePolicyTests::on_apply_keepsTextThatFits()
{
  const Policy policy(true);
  Clipboard clipboard;
  fill(clipboard, static_cast<int>(kMaxText), false, false);
  const auto before = clipboard.marshall();

  const auto decision = policy.apply(clipboard);
  QCOMPARE(static_cast<int>(decision.action), static_cast<int>(Policy::Action::ShareText));
  QCOMPARE(clipboard.marshall(), before);
  QCOMPARE(before.size(), kLimit);
}

void ClipboardSharePolicyTests::sizeOption_data()
{
  QTest::addColumn<bool>("textOnly");
  QTest::addColumn<quint32>("configuredMiB");
  QTest::addColumn<quint32>("expectedKiB");

  // off: the configured size in KiB, as it always was
  QTest::newRow("off, 0 MiB") << false << 0u << 0u;
  QTest::newRow("off, 1 MiB") << false << 1u << 1024u;
  QTest::newRow("off, 3 MiB") << false << 3u << 3072u;
  QTest::newRow("off, 20 MiB") << false << 20u << 20480u;
  // on: 10 KiB at most, and 0 (sharing is off) stays 0
  QTest::newRow("on, 0 MiB") << true << 0u << 0u;
  QTest::newRow("on, 1 MiB") << true << 1u << 10u;
  QTest::newRow("on, 3 MiB") << true << 3u << 10u;
  QTest::newRow("on, 20 MiB") << true << 20u << 10u;
}

void ClipboardSharePolicyTests::sizeOption()
{
  QFETCH(bool, textOnly);
  QFETCH(quint32, configuredMiB);
  QFETCH(quint32, expectedKiB);

  QCOMPARE(Policy::sizeOptionKiB(configuredMiB, textOnly), expectedKiB);
}

void ClipboardSharePolicyTests::rejectionLog_reportsEachRejectionOnce()
{
  using enum Policy::Action;
  using enum Policy::Reason;
  const Policy::Decision shared{ShareText, None, 5};
  const Policy::Decision tooLarge{Drop, TextTooLarge, 20000};
  const Policy::Decision longer{Drop, TextTooLarge, 30000};
  const Policy::Decision noText{Drop, NoText, 0};

  ClipboardRejectionLog log;
  QVERIFY2(!log.report(shared), "a clipboard that is shared is nothing to report");
  QVERIFY(!log.report(Policy::Decision{}));

  QVERIFY2(log.report(tooLarge), "the first rejection is reported");
  // every switch to another computer reads the clipboard again
  for (int i = 0; i < 50; ++i)
    QVERIFY2(!log.report(tooLarge), "the same rejection again is not");

  QVERIFY2(log.report(longer), "another text is a new rejection");
  QVERIFY2(log.report(noText), "so is another reason");
  QVERIFY(!log.report(noText));

  // a clipboard that is shared re-arms the report
  QVERIFY(!log.report(shared));
  QVERIFY(log.report(noText));
}

void ClipboardSharePolicyTests::clipboard_textOnly_ignoresOtherFormats()
{
  Clipboard clipboard;
  QVERIFY(clipboard.wants(Format::HTML));
  QVERIFY(clipboard.wants(Format::Bitmap));

  clipboard.setTextOnly(true);
  QVERIFY(clipboard.wants(Format::Text));
  QVERIFY(!clipboard.wants(Format::HTML));
  QVERIFY(!clipboard.wants(Format::Bitmap));

  fill(clipboard, 7, true, true);
  Clipboard textOnly;
  fill(textOnly, 7, false, false);
  QCOMPARE(clipboard.marshall(), textOnly.marshall());

  clipboard.setTextOnly(false);
  QVERIFY(clipboard.wants(Format::HTML));
  QVERIFY(clipboard.wants(Format::Bitmap));
  fill(clipboard, 7, true, true);
  QVERIFY(clipboard.open(0));
  QVERIFY(clipboard.has(Format::HTML));
  QVERIFY(clipboard.has(Format::Bitmap));
  clipboard.close();
}

// Only what is added from then on is held to it
void ClipboardSharePolicyTests::clipboard_textOnly_keepsWhatItHolds()
{
  Clipboard clipboard;
  fill(clipboard, 7, true, false);
  clipboard.setTextOnly(true);

  QVERIFY(clipboard.open(0));
  QVERIFY(clipboard.has(Format::HTML));
  clipboard.close();
}

void ClipboardSharePolicyTests::clipboard_textOnly_unmarshallsTextOnly()
{
  Clipboard everything;
  fill(everything, 7, true, true);
  Clipboard textOnly;
  fill(textOnly, 7, false, false);

  Clipboard received;
  received.setTextOnly(true);
  received.unmarshall(everything.marshall(), 0);
  QCOMPARE(received.marshall(), textOnly.marshall());
}

// Reading is what costs: a platform converts an image to a bitmap only when it is asked for it
void ClipboardSharePolicyTests::copy_intoTextOnlyClipboard_neverReadsOtherFormats()
{
  SpyClipboard source;
  Clipboard destination;
  destination.setTextOnly(true);

  QVERIFY(IClipboard::copy(&destination, &source));

  QCOMPARE(source.getCalls(Format::Text), 1);
  QCOMPARE(source.getCalls(Format::HTML), 0);
  QCOMPARE(source.getCalls(Format::Bitmap), 0);
  QCOMPARE(source.hasCalls(Format::HTML), 0);
  QCOMPARE(source.hasCalls(Format::Bitmap), 0);

  QVERIFY(destination.open(0));
  QVERIFY(destination.has(Format::Text));
  QVERIFY(!destination.has(Format::HTML));
  QVERIFY(!destination.has(Format::Bitmap));
  destination.close();
}

void ClipboardSharePolicyTests::copy_intoDefaultClipboard_readsEveryFormat()
{
  SpyClipboard source;
  Clipboard destination;

  QVERIFY(IClipboard::copy(&destination, &source));

  QCOMPARE(source.getCalls(Format::Text), 1);
  QCOMPARE(source.getCalls(Format::HTML), 1);
  QCOMPARE(source.getCalls(Format::Bitmap), 1);

  QVERIFY(destination.open(0));
  QVERIFY(destination.has(Format::Text));
  QVERIFY(destination.has(Format::HTML));
  QVERIFY(destination.has(Format::Bitmap));
  destination.close();
}

QTEST_MAIN(ClipboardSharePolicyTests)
