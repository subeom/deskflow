/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/core/CoreLogText.h"

#include <QCoreApplication>
#include <QList>

namespace deskflow::gui {

namespace {

// The English that the core writes (formatPointerHoldLog() in src/lib/platform/OSXPointerHold.cpp) and its translation.
// The two are written out twice on purpose: lupdate reads the literal of translate(), and the line of the core is
// matched with the English; the test compares them.
struct KnownLine
{
  QString english;
  QString translated;
};

QList<KnownLine> knownLines()
{
  return {
      {QStringLiteral("the pointer of this Mac moved while it is on \"%1\": held it again"),
       QCoreApplication::translate(
           "CoreLogText", "the pointer of this Mac moved while it is on \"%1\": held it again"
       )},
      {QStringLiteral("the pointer of this Mac was shown while it is on \"%1\": hid it again"),
       QCoreApplication::translate(
           "CoreLogText", "the pointer of this Mac was shown while it is on \"%1\": hid it again"
       )},
      {QStringLiteral("the event tap was off while the pointer is on \"%1\": switched it on again"),
       QCoreApplication::translate(
           "CoreLogText", "the event tap was off while the pointer is on \"%1\": switched it on again"
       )},
  };
}

} // namespace

QString localizeCoreLogLine(const QString &line)
{
  // only the lines of the hold of the pointer are known, and only a macOS server writes them: look before translating
  if (!line.contains(QLatin1String("pointer")) && !line.contains(QLatin1String("event tap"))) {
    return line;
  }

  const auto marker = QStringLiteral("%1");
  const auto lines = knownLines();
  for (const auto &known : lines) {
    const auto at = known.english.indexOf(marker);
    const auto head = known.english.left(at);
    const auto tail = known.english.mid(at + marker.size());

    const auto headAt = line.indexOf(head);
    if (headAt < 0) {
      continue;
    }
    const auto nameAt = headAt + head.size();
    const auto tailAt = line.indexOf(tail, nameAt);
    if (tailAt < 0) {
      continue;
    }
    if (known.translated == known.english) {
      // no translation of the sentence: the line stays as the core wrote it
      return line;
    }
    const auto name = line.mid(nameAt, tailAt - nameAt);
    return line.left(headAt) + known.translated.arg(name) + line.mid(tailAt + tail.size());
  }
  return line;
}

} // namespace deskflow::gui
