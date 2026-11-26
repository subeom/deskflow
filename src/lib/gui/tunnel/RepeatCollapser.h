/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QtGlobal>

namespace deskflow::tunnel {

/**
 * Keeps a line that repeats out of the log. While the tunnel of the server is down every try of the client makes
 * cloudflared write the same line ("failed to connect to origin"), a few times a minute for as long as it lasts. The
 * first one is shown; the same line again within \p windowMs is counted, and when the window is over (or another line
 * comes) the log says how many were left out.
 *
 * It decides only what is shown: whoever feeds it still reads every line (the state of the tunnel does).
 */
class RepeatCollapser
{
public:
  struct Result
  {
    bool show = false; //!< the line is put in the log
    QString summary;   //!< what was left out before it, to put in the log first; empty when nothing was
  };

  explicit RepeatCollapser(qint64 windowMs = 30'000) : m_windowMs(windowMs)
  {
  }

  //! \p key says what the line is (the line without the time at its start); \p nowMs is the time of the line
  Result feed(const QString &key, qint64 nowMs);

  //! The summary of what was left out and was not told yet; empty when nothing was. Whatever comes next is shown.
  QString flush(qint64 nowMs);

  void reset();

private:
  QString summaryText(qint64 nowMs) const;

  qint64 m_windowMs;
  QString m_key;
  qint64 m_shownAt = 0;
  int m_hidden = 0;
  bool m_has = false;
};

} // namespace deskflow::tunnel
