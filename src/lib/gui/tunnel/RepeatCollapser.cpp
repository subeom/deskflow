/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/RepeatCollapser.h"

#include <algorithm>

namespace deskflow::tunnel {

QString RepeatCollapser::summaryText(qint64 nowMs) const
{
  if (m_hidden <= 0) {
    return {};
  }
  const auto seconds = std::max<qint64>(1, (nowMs - m_shownAt + 500) / 1000);
  return QStringLiteral("The line above came %1 more %2 in the last %3 s")
      .arg(m_hidden)
      .arg(m_hidden == 1 ? QStringLiteral("time") : QStringLiteral("times"))
      .arg(seconds);
}

RepeatCollapser::Result RepeatCollapser::feed(const QString &key, qint64 nowMs)
{
  if (m_has && key == m_key && nowMs - m_shownAt < m_windowMs) {
    ++m_hidden;
    return {};
  }

  Result result;
  result.show = true;
  result.summary = summaryText(nowMs);
  m_key = key;
  m_shownAt = nowMs;
  m_hidden = 0;
  m_has = true;
  return result;
}

QString RepeatCollapser::flush(qint64 nowMs)
{
  const auto summary = summaryText(nowMs);
  // the line that was shown is over: the next one, the same or not, is shown
  m_hidden = 0;
  m_has = false;
  return summary;
}

void RepeatCollapser::reset()
{
  m_key.clear();
  m_hidden = 0;
  m_shownAt = 0;
  m_has = false;
}

} // namespace deskflow::tunnel
