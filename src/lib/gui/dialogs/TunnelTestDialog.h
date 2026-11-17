/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/TunnelTestRunner.h"

#include <QDateTime>
#include <QDialog>

#include <functional>

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QTextBrowser;

/**
 * "Test connection" while the client uses a Cloudflare Tunnel: the stages of the way to the server (cloudflared, the
 * Service Token, TLS, the hostname, the process, the local port, a connection through the tunnel) one under the other,
 * the first that does not work marked, and what to do about it. See deskflow::tunnel::TunnelTestRunner for what is
 * tested; no Deskflow data is sent.
 */
class TunnelTestDialog : public QDialog
{
  Q_OBJECT

public:
  using ClipboardWriter = std::function<void(const QString &text)>;

  //! \p dependencies names the controller of the tunnel; the rest is for tests (the system's by default)
  explicit TunnelTestDialog(
      QWidget *parent, deskflow::tunnel::TunnelTestRunner::Dependencies dependencies,
      ClipboardWriter clipboardWriter = {}
  );

  //! The line that is copied
  QString summary() const;
  bool isTesting() const;

  //! What to do about a failure, in the language of the application; empty for none
  static QString hintText(deskflow::tunnel::TunnelFailure failure);

public Q_SLOTS:
  void runTest();
  void copySummary();

protected:
  void changeEvent(QEvent *event) override;

public:
  QSize sizeHint() const override;

private:
  void retranslate();
  void showReport();

  deskflow::tunnel::TunnelTestRunner *m_runner;
  ClipboardWriter m_clipboardWriter;
  QDateTime m_startedAt;

  QTextBrowser *m_resultsView;
  QLabel *m_status;
  QPushButton *m_copyButton;
  QPushButton *m_runAgainButton;
  QDialogButtonBox *m_buttons;
};
