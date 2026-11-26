/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace deskflow::tunnel {

/**
 * Cuts what a process writes into lines (a line ends with \n, \r or \r\n) and gives only whole lines. The bytes of one
 * line can arrive in any number of reads, so a Service Token in the middle of a line can be cut in two reads: the line
 * is kept until it is whole, and only then given, so that whoever masks the token sees all of it.
 *
 * A line that is longer than the limit is **not** given, not even in parts: a part of a token (the part before or after
 * the cut at the limit) would pass a mask that looks for the whole token. What was kept of it is dropped, the bytes
 * that follow are dropped up to the end of that line, and one fixed notice line (kTooLongNotice) is given in its place.
 */
class OutputLineSplitter
{
public:
  static constexpr qsizetype kDefaultMaxLineBytes = 64 * 1024;
  static constexpr auto kTooLongNotice = "cloudflared output line too long, skipped";

  explicit OutputLineSplitter(qsizetype maxLineBytes = kDefaultMaxLineBytes);

  //! The lines that \p data completes (and the notice for a line that passed the limit); empty lines are not given
  QStringList feed(const QByteArray &data);
  //! The end of the output: the last line, which had no end, is given (it is whole: nothing follows it). A line that
  //! was being skipped is not.
  QStringList finish();

private:
  void giveLine(QStringList &lines);

  qsizetype m_max;
  QByteArray m_pending;
  bool m_skipping = false;
};

} // namespace deskflow::tunnel
