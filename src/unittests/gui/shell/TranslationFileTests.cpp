/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TranslationFileTests.h"

#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

namespace {

struct Message
{
  QString context;
  QString source;
  QString comment;
  QString translation;
  QStringList forms; // of a text with a plural
  QString type;      // unfinished, vanished, obsolete or nothing

  QString name() const
  {
    auto text = source.simplified();
    if (text.size() > 70)
      text = text.left(70) + QStringLiteral("...");
    return QStringLiteral("%1 | %2").arg(context, text);
  }
  // the texts of the translation: one, or one for each plural form
  QStringList texts() const
  {
    return forms.isEmpty() ? QStringList{translation} : forms;
  }
  bool isLive() const
  {
    return type != QLatin1String("vanished") && type != QLatin1String("obsolete");
  }
  // a message that has its Korean (the first test names the ones that have not)
  bool isTranslated() const
  {
    return isLive() && type != QLatin1String("unfinished") && !translation.trimmed().isEmpty();
  }
};

QList<Message> readFile()
{
  QList<Message> messages;
  QFile file(QStringLiteral(DESKFLOW_KO_TS));
  if (!file.open(QIODevice::ReadOnly))
    return messages;
  QXmlStreamReader xml(&file);
  QString context;
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement())
      continue;
    if (xml.name() == QLatin1String("context")) {
      context.clear();
    } else if (xml.name() == QLatin1String("name") && context.isEmpty()) {
      context = xml.readElementText();
    } else if (xml.name() == QLatin1String("message")) {
      Message message;
      message.context = context;
      while (!(xml.isEndElement() && xml.name() == QLatin1String("message")) && !xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
          continue;
        if (xml.name() == QLatin1String("source")) {
          message.source = xml.readElementText(QXmlStreamReader::IncludeChildElements);
        } else if (xml.name() == QLatin1String("comment")) {
          message.comment = xml.readElementText(QXmlStreamReader::IncludeChildElements);
        } else if (xml.name() == QLatin1String("translation")) {
          message.type = xml.attributes().value(QLatin1String("type")).toString();
          while (!(xml.isEndElement() && xml.name() == QLatin1String("translation")) && !xml.atEnd()) {
            xml.readNext();
            if (xml.isCharacters())
              message.translation += xml.text();
            else if (xml.isStartElement() && xml.name() == QLatin1String("numerusform"))
              message.forms << xml.readElementText(QXmlStreamReader::IncludeChildElements);
          }
        }
      }
      messages.append(message);
    }
  }
  return messages;
}

QStringList places(const QString &text)
{
  QStringList found;
  for (auto it = QRegularExpression(QStringLiteral("%(?:n|\\d)")).globalMatch(text); it.hasNext();)
    found << it.next().captured();
  found.removeDuplicates();
  found.sort();
  return found;
}

// the key of a menu: an & in front of a letter or a digit (a && is an &, and an & before a space is an &)
int keys(const QString &text)
{
  auto plain = text;
  plain.replace(QStringLiteral("&&"), QString());
  return plain.count(QRegularExpression(QStringLiteral("&(?=[A-Za-z0-9])")));
}

QStringList tagsOf(const QString &text)
{
  QStringList found;
  if (!text.contains(QLatin1Char('<')))
    return found;
  for (auto it = QRegularExpression(QStringLiteral("</?\\s*([A-Za-z0-9]+)")).globalMatch(text); it.hasNext();)
    found << it.next().captured(1);
  return found;
}

// The two messages of the translators that were in the file before this work: one lost a mark of the HTML that closes a
// paragraph. They are theirs and are not changed here
bool isOfTheTranslatorsWithAMissingMark(const Message &message)
{
  return message.source.startsWith(QLatin1String("<p>Failed to connect to the server '%1'.</p><p>A Client with")) ||
         message.source.startsWith(QLatin1String("<html><head/><body><p>Enables the network heartbeat to ping "));
}

QString lines(const QStringList &names)
{
  return QStringLiteral("(%1):\n  %2").arg(names.size()).arg(names.join(QStringLiteral("\n  ")));
}

} // namespace

void TranslationFileTests::file_hasAKoreanTextForEveryTextOfTheProgram()
{
  const auto messages = readFile();
  QVERIFY2(messages.size() > 700, qPrintable(QString::number(messages.size())));
  QStringList without;
  for (const auto &message : messages) {
    if (!message.isLive())
      continue;
    const auto texts = message.texts();
    const bool empty = std::any_of(texts.begin(), texts.end(), [](const QString &t) { return t.trimmed().isEmpty(); });
    if (message.type == QLatin1String("unfinished") || empty)
      without << message.name();
  }
  without.sort();
  QVERIFY2(without.isEmpty(), qPrintable(QStringLiteral("without a Korean text ") + lines(without)));
}

void TranslationFileTests::file_keepsThePlacesOfTheValues()
{
  QStringList wrong;
  for (const auto &message : readFile()) {
    if (!message.isTranslated() || !message.forms.isEmpty())
      continue;
    if (places(message.source) != places(message.translation))
      wrong << message.name();
  }
  wrong.sort();
  QVERIFY2(
      wrong.isEmpty(), qPrintable(QStringLiteral("places of the values that are not the English's ") + lines(wrong))
  );
}

void TranslationFileTests::file_keepsTheKeysOfTheMenus()
{
  QStringList wrong;
  for (const auto &message : readFile()) {
    if (!message.isTranslated() || !message.forms.isEmpty())
      continue;
    if (keys(message.source) != keys(message.translation))
      wrong << message.name();
  }
  wrong.sort();
  QVERIFY2(wrong.isEmpty(), qPrintable(QStringLiteral("keys of a menu that are not the English's ") + lines(wrong)));
}

void TranslationFileTests::file_keepsTheBreaksOfTheLinesAndTheSpacesAtTheEnds()
{
  QStringList wrong;
  for (const auto &message : readFile()) {
    if (!message.isTranslated() || !message.forms.isEmpty())
      continue;
    const auto &s = message.source;
    const auto &t = message.translation;
    const bool sameBreaks = s.count(QLatin1Char('\n')) == t.count(QLatin1Char('\n'));
    const bool sameEnds = s.front().isSpace() == t.front().isSpace() && s.back().isSpace() == t.back().isSpace();
    if (!sameBreaks || !sameEnds)
      wrong << message.name();
  }
  wrong.sort();
  QVERIFY2(wrong.isEmpty(), qPrintable(QStringLiteral("breaks or spaces that are not the English's ") + lines(wrong)));
}

void TranslationFileTests::file_keepsTheMarksOfTheHtml()
{
  QStringList wrong;
  for (const auto &message : readFile()) {
    if (!message.isTranslated() || !message.forms.isEmpty() || isOfTheTranslatorsWithAMissingMark(message))
      continue;
    if (tagsOf(message.source) != tagsOf(message.translation))
      wrong << message.name();
  }
  wrong.sort();
  QVERIFY2(wrong.isEmpty(), qPrintable(QStringLiteral("marks of the HTML that are not the English's ") + lines(wrong)));
}

void TranslationFileTests::file_hasNoEmDash()
{
  // the style of the Korean texts: a label and its words are written with a colon, not a long dash
  QStringList wrong;
  for (const auto &message : readFile()) {
    for (const auto &text : message.texts()) {
      if (text.contains(QChar(0x2014)))
        wrong << message.name();
    }
  }
  wrong.sort();
  QVERIFY2(wrong.isEmpty(), qPrintable(QStringLiteral("an em dash ") + lines(wrong)));
}

void TranslationFileTests::file_hasTheTextsThatTheRedesignAdded()
{
  // some of the texts of the pages of the settings are written by a helper that lupdate cannot read
  // (SettingsPages.cpp): each is marked where it is used, and each has its Korean here. A few from each part of the new
  // look
  const QList<QPair<QString, QString>> wanted{
      {QStringLiteral("deskflow::gui::MainShell"), QStringLiteral("Home")},
      {QStringLiteral("deskflow::gui::MainShell"), QStringLiteral("Share this computer")},
      {QStringLiteral("deskflow::gui::StatusModel"), QStringLiteral("Waiting for clients")},
      {QStringLiteral("deskflow::gui::chrome::WindowHeader"), QStringLiteral("Maximize")},
      {QStringLiteral("deskflow::gui::settings::RestartBanner"), QStringLiteral("Restart sharing")},
      {QStringLiteral("deskflow::gui::tray::TrayMenuModel"), QStringLiteral("Needs attention")},
      {QStringLiteral("SettingsPages"), QStringLiteral("Look of the application")},
      {QStringLiteral("SettingsPages"), QStringLiteral("Text size")},
      {QStringLiteral("SettingsPages"), QStringLiteral("Input source indicator")},
      {QStringLiteral("SettingsPages"),
       QStringLiteral("한/A always beside the mouse pointer while the pointer is local")},
      {QStringLiteral("SettingsPages"), QStringLiteral("Position from cursor")},
      {QStringLiteral("SettingsPages"),
       QStringLiteral("Shows the current input-source badge beside the local mouse pointer. "
                      "It reads only the input-source identifier and pointer position; it does not read Accessibility "
                      "focus or text, key input, caret contents, clipboard contents, application titles, URLs, "
                      "selections, values, or screenshots.")},
      {QStringLiteral("SettingsPages"), QStringLiteral("Use the Deskflow window frame")},
      {QStringLiteral("SettingsPages"), QStringLiteral("Security & clipboard")},
      {QStringLiteral("TunnelChip"), QStringLiteral("Tunnel: connected")},
      {QStringLiteral("TestResultRows"), QStringLiteral("What to try")},
      {QStringLiteral("deskflow::gui::TunnelSettingsGroup"), QStringLiteral("Connect through Cloudflare Tunnel")},
      {QStringLiteral("ConnectionTestDialog"), QStringLiteral("Test connection")},
      {QStringLiteral("TunnelTestDialog"), QStringLiteral("Test connection (Cloudflare Tunnel)")},
      {QStringLiteral("ComputerSettingsDialog"), QStringLiteral("Share clipboard with this computer")},
  };
  const auto messages = readFile();
  QStringList missing;
  for (const auto &[context, source] : wanted) {
    const auto found = std::any_of(messages.begin(), messages.end(), [&](const Message &m) {
      return m.context == context && m.source == source && m.isLive() && m.type.isEmpty() &&
             !m.translation.trimmed().isEmpty();
    });
    if (!found)
      missing << context + QStringLiteral(" | ") + source;
  }
  QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("not translated ") + lines(missing)));
}

QTEST_MAIN(TranslationFileTests)
