/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// A stand-in for `cloudflared access tcp --hostname <host> --url 127.0.0.1:<port> --loglevel info` that the tests of
// the tunnel start instead of the real program. It never talks to the network beyond the local port it is told to
// listen on, and it ends by itself after a minute so that a test that was aborted leaves nothing behind.
//
// What it does depends on the first label of the host name (the host names are placeholders, nothing is looked up):
//   ok.        listens on the port and holds every connection that is opened
//   silent.    runs and writes nothing, never listens
//   login.     writes what cloudflared writes when Cloudflare Access asks for a login, never listens
//   exit.      writes an error and stops with the code 3 at once
//   flaky.     listens, then stops with the code 2 after a short time
//   deny.      listens, and closes every connection after writing what cloudflared writes when the edge refuses it
//   leak.      like ok. and also writes the Service Token (to prove that what it writes is made safe by the tunnel)
//   noport.    runs and writes that it listens, never does
//   long.      writes one line of 200 KB with the Service Token across the 64 KB mark (and in other places), then a
//   line
//              that says "after the long line", and runs
//   split.     writes a line whose token is cut between two writes with a pause between them, then runs
//   cr.        writes three lines that end with a carriage return alone, then runs
//   noend.     writes the token as the last words without an end of line, and stops with the code 3
//
// The first line it writes says what it found in its environment and arguments; the value of the secret never appears
// in it (only the leak. mode writes it, on purpose).

#include <QCoreApplication>
#include <QHostAddress>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <cstdio>

namespace {

void say(const QString &text)
{
  std::fprintf(stdout, "%s\n", text.toUtf8().constData());
  std::fflush(stdout);
}

QString argumentAfter(const QStringList &arguments, const QString &name)
{
  const auto index = arguments.indexOf(name);
  return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString();
}

} // namespace

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);
  const auto arguments = app.arguments().mid(1);

  const auto hostname = argumentAfter(arguments, QStringLiteral("--hostname"));
  const auto url = argumentAfter(arguments, QStringLiteral("--url"));
  const auto mode = hostname.section(QLatin1Char('.'), 0, 0);
  const auto port = static_cast<quint16>(url.section(QLatin1Char(':'), -1).toUInt());

  const auto environment = QProcessEnvironment::systemEnvironment();
  const auto id = environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_ID"));
  const auto secret = environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET"));

  bool secretInArguments = false;
  for (const auto &argument : arguments) {
    if (!secret.isEmpty() && argument.contains(secret)) {
      secretInArguments = true;
    }
    if (!id.isEmpty() && argument.contains(id)) {
      secretInArguments = true;
    }
  }

  int strayTunnelVariables = 0;
  for (const auto &name : environment.keys()) {
    if (name.startsWith(QLatin1String("TUNNEL_")) && name != QLatin1String("TUNNEL_SERVICE_TOKEN_ID") &&
        name != QLatin1String("TUNNEL_SERVICE_TOKEN_SECRET")) {
      ++strayTunnelVariables;
    }
  }

  say(QStringLiteral("fake: mode=%1 port=%2 tokenId=%3 tokenSecret=%4 tokenInArguments=%5 strayTunnelVariables=%6")
          .arg(mode)
          .arg(port)
          .arg(id.isEmpty() ? 0 : 1)
          .arg(secret.isEmpty() ? 0 : 1)
          .arg(secretInArguments ? 1 : 0)
          .arg(strayTunnelVariables));

  // a test that was aborted must not leave a process behind
  QTimer::singleShot(60000, &app, &QCoreApplication::quit);

  if (mode == QLatin1String("exit")) {
    say(QStringLiteral("2026-01-01T00:00:00Z ERR fake: the connection to the edge failed"));
    return 3;
  }

  if (mode == QLatin1String("long")) {
    QByteArray line(200 * 1024, 'a');
    const auto token = secret.toUtf8();
    for (const qsizetype at :
         {qsizetype(65536 - 10), qsizetype(65536), qsizetype(65536 - token.size() + 3), qsizetype(100 * 1024),
          qsizetype(150 * 1024)}) {
      line.replace(at, token.size(), token);
    }
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
    say(QString());
    say(QStringLiteral("after the long line"));
    return app.exec();
  }

  if (mode == QLatin1String("split")) {
    const auto token = secret.toUtf8();
    const auto half = token.size() / 2;
    std::fprintf(stdout, "ERR token cut in two: %s", token.left(half).constData());
    std::fflush(stdout);
    QThread::msleep(300);
    std::fprintf(stdout, "%s end of the line\n", token.mid(half).constData());
    std::fflush(stdout);
    return app.exec();
  }

  if (mode == QLatin1String("cr")) {
    std::fprintf(stdout, "cr one\rcr two\rcr three\n");
    std::fflush(stdout);
    return app.exec();
  }

  if (mode == QLatin1String("noend")) {
    std::fprintf(stdout, "ERR the last words, with the secret %s", secret.toUtf8().constData());
    std::fflush(stdout);
    return 3;
  }

  if (mode == QLatin1String("silent")) {
    return app.exec();
  }

  if (mode == QLatin1String("login")) {
    say(QStringLiteral("2026-01-01T00:00:00Z INF A browser window should have opened at the following URL:"));
    say(QStringLiteral("https://example.invalid/cdn-cgi/access/cli?token=NOT-A-REAL-TOKEN"));
    say(QStringLiteral("If the browser failed to open, please visit the URL above directly in your browser."));
    say(QStringLiteral("Waiting for login..."));
    return app.exec();
  }

  if (mode == QLatin1String("noport")) {
    say(QStringLiteral("2026-01-01T00:00:00Z INF Start Websocket listener host=127.0.0.1:%1").arg(port));
    return app.exec();
  }

  QTcpServer server;
  say(QStringLiteral("2026-01-01T00:00:00Z INF Start Websocket listener host=127.0.0.1:%1").arg(port));
  if (!server.listen(QHostAddress::LocalHost, port)) {
    say(QStringLiteral(
            "2026-01-01T00:00:00Z ERR failed to start forwarding server error=\"listen tcp 127.0.0.1:%1: "
            "bind: address already in use\""
    )
            .arg(port));
    return 1;
  }

  if (mode == QLatin1String("leak")) {
    say(QStringLiteral("2026-01-01T00:00:00Z ERR Cf-Access-Client-Secret: %1").arg(secret));
    say(QStringLiteral("2026-01-01T00:00:00Z ERR the id is %1 and the secret is %2").arg(id, secret));
  }

  if (mode == QLatin1String("flaky")) {
    QTimer::singleShot(400, &app, [&app] { app.exit(2); });
  }

  QObject::connect(&server, &QTcpServer::newConnection, &app, [&server, mode] {
    while (auto *socket = server.nextPendingConnection()) {
      if (mode == QLatin1String("deny")) {
        say(QStringLiteral("2026-01-01T00:00:01Z ERR failed to connect to origin error=\"websocket: bad handshake\""));
        socket->close();
        socket->deleteLater();
        continue;
      }
      QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
  });

  return app.exec();
}
