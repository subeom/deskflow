/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/OutputLineSplitter.h"

namespace deskflow::tunnel {

OutputLineSplitter::OutputLineSplitter(qsizetype maxLineBytes) : m_max(maxLineBytes)
{
}

void OutputLineSplitter::giveLine(QStringList &lines)
{
  const auto line = QString::fromUtf8(m_pending).trimmed();
  m_pending.clear();
  if (!line.isEmpty()) {
    lines << line;
  }
}

QStringList OutputLineSplitter::feed(const QByteArray &data)
{
  QStringList lines;

  qsizetype position = 0;
  while (position < data.size()) {
    qsizetype end = position;
    while (end < data.size() && data.at(end) != '\n' && data.at(end) != '\r') {
      ++end;
    }

    // the piece of the line that is in this read
    const auto length = end - position;
    if (!m_skipping) {
      if (m_pending.size() + length > m_max) {
        m_pending.clear();
        m_skipping = true;
        lines << QString::fromLatin1(kTooLongNotice);
      } else {
        m_pending.append(data.constData() + position, length);
      }
    }

    if (end == data.size()) {
      break;
    }

    // the end of the line
    if (m_skipping) {
      m_skipping = false;
    } else {
      giveLine(lines);
    }
    position = end + 1;
  }

  return lines;
}

QStringList OutputLineSplitter::finish()
{
  QStringList lines;
  if (!m_skipping) {
    giveLine(lines);
  }
  m_pending.clear();
  m_skipping = false;
  return lines;
}

} // namespace deskflow::tunnel
