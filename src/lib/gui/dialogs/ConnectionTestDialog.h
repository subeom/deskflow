/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/ConnectionDiagnosis.h"
#include "gui/core/ConnectionProbe.h"
#include "gui/widgets/TestResultRows.h"

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QString>

#include <functional>

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTextBrowser;

/**
 * Shows, for every address in the client's "Connect to" field, whether this computer can open a TCP connection to
 * it, and what it most likely means when it cannot. Only a connection is made and closed again: no Deskflow data is
 * sent.
 */
class ConnectionTestDialog : public QDialog
{
  Q_OBJECT

public:
  using ProbeFactory = std::function<deskflow::gui::ConnectionProbe *(QObject *parent)>;
  using ClipboardWriter = std::function<void(const QString &text)>;

  /**
   * \p servers is the text of the field, a comma separated list. \p probeFactory and \p clipboardWriter are there for
   * tests: by default the dialog tests with the system's sockets and copies to the system clipboard.
   */
  explicit ConnectionTestDialog(
      QWidget *parent, const QString &servers, int defaultPort, ProbeFactory probeFactory = {},
      ClipboardWriter clipboardWriter = {}
  );

  //! The one line summary of the last test, to paste into a message to IT
  QString summary() const;
  bool isTesting() const;

  //! The sentence of a hint, in the language of the application
  static QString hintText(deskflow::diagnosis::Hint hint, int port, double timeoutSeconds);
  //! What came of one address, in a sentence, in the language of the application
  static QString statusText(const deskflow::gui::ProbeResult &result, double clientTimeoutSeconds);
  //! The sentences of what to try for a result that did not open (the address is not one that failed: none); the
  //! probe waited \p probeTimeoutSeconds
  static QStringList hintTexts(const deskflow::gui::ProbeResult &result, double probeTimeoutSeconds);
  //! The rows that the new look shows for the results: the same words as the text of the system look
  static QList<deskflow::gui::TestResultRows::Row>
  rowsFor(const QList<deskflow::gui::ProbeResult> &results, double probeTimeoutSeconds, double clientTimeoutSeconds);

public Q_SLOTS:
  //! Starts the test again; the results of a test that is running are dropped
  void runTest();
  void copySummary();

protected:
  void changeEvent(QEvent *event) override;

public:
  //! Room for the panel to show the hints of a few addresses without scrolling: lines and letters of the font in use
  QSize sizeHint() const override;

private:
  void retranslate();
  void showResults();
  void showRows(bool noAddress, bool testing);
  QString resultHtml(const deskflow::gui::ProbeResult &result) const;

  QString m_servers;
  int m_defaultPort;
  ProbeFactory m_probeFactory;
  ClipboardWriter m_clipboardWriter;

  deskflow::gui::ConnectionProbe *m_probe = nullptr;
  QList<deskflow::gui::ProbeResult> m_results;
  QDateTime m_startedAt;
  bool m_finished = false;
  double m_clientTimeoutSeconds = deskflow::diagnosis::kDefaultConnectTimeoutSeconds;

  QTextBrowser *m_resultsView;
  //! The new look shows the results as rows (in a scroll area) in the place of the text of the system look
  deskflow::gui::TestResultRows *m_rows = nullptr;
  QScrollArea *m_rowsScroll = nullptr;
  QLabel *m_status;
  QPushButton *m_copyButton;
  QPushButton *m_runAgainButton;
  QDialogButtonBox *m_buttons;
};
