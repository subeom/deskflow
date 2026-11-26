/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// The manual check of the secure store of the system (the keychain of macOS, the Credential Manager of Windows). It is NOT a
// test: no test of Deskflow calls the store of the system, because a call can ask the user for a password. It is built only
// when the CMake option DESKFLOW_BUILD_SECRET_SMOKE is ON, and it does nothing unless it is given the flag that says that it
// is meant to use the real store.
//
// It never touches the Service Token of the user: it uses a name of its own (a random one for every run) and made-up
// values, writes, reads, replaces and removes an item of that name, and removes it again at the end whatever happened. It
// never prints the Secret that it writes.

#include "gui/tunnel/MacKeychainStore.h"
#include "gui/tunnel/WindowsCredentialStore.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QUuid>

#include <memory>

using namespace deskflow::tunnel;

namespace {

constexpr auto kFlag = "--use-the-real-store";

int g_failed = 0;

void report(QTextStream &out, bool ok, const QString &step, const QString &detail = {})
{
  out << (ok ? "PASS  " : "FAIL  ") << step;
  if (!ok && !detail.isEmpty()) {
    out << ": " << detail;
  }
  out << Qt::endl;
  if (!ok) {
    ++g_failed;
  }
}

} // namespace

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);
  QTextStream out(stdout);

  if (!app.arguments().contains(QString::fromLatin1(kFlag))) {
    out << "This program writes an item with a made-up name to the real secure store of this computer, and removes it again.\n"
        << "The system may ask for your password: that is part of what is checked. The Service Token of Deskflow is not "
           "touched.\n"
        << "Run it with " << kFlag << " to go on." << Qt::endl;
    return 2;
  }

  const auto run = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
  std::unique_ptr<ISecretStore> store;
#if defined(Q_OS_MACOS)
  store = std::make_unique<MacKeychainStore>(QStringLiteral("com.deskflow.smoketest.%1").arg(run));
#elif defined(Q_OS_WIN)
  store = std::make_unique<WindowsCredentialStore>(QStringLiteral("Deskflow/SmokeTest/%1").arg(run));
#else
  out << "This computer has no persistent secure store that Deskflow uses: nothing to check." << Qt::endl;
  return 0;
#endif

  out << "store: " << store->name() << ", item of this run: " << run << Qt::endl;
  report(out, store->isPersistent(), QStringLiteral("the store is persistent"));

  const ServiceToken first(QStringLiteral("smoketest-id-%1.access").arg(run), QStringLiteral("smoketest-secret-%1-aaaa").arg(run));
  const ServiceToken second(QStringLiteral("smoketest-id-%1-b.access").arg(run), QStringLiteral("smoketest-secret-%1-bbbb").arg(run));

  StoreResult result;
  store->storedClientId(&result);
  report(out, result.notFound, QStringLiteral("nothing is stored under the name of this run"), result.detail);

  result = store->save(first);
  report(out, result.ok, QStringLiteral("save"), result.detail);

  const auto id = store->storedClientId(&result);
  report(out, id.has_value() && *id == first.clientId, QStringLiteral("the Client ID is read back without the Secret"), result.detail);

  const auto loaded = store->load(&result);
  report(out, loaded.has_value() && loaded->clientId == first.clientId && loaded->secret == first.secret, QStringLiteral("the whole token is read back"), result.detail);

  result = store->save(second);
  report(out, result.ok, QStringLiteral("save of another token replaces the first"), result.detail);
  const auto replaced = store->load(&result);
  report(out, replaced.has_value() && replaced->clientId == second.clientId && replaced->secret == second.secret, QStringLiteral("only the second token is stored"), result.detail);

  result = store->remove();
  report(out, result.ok, QStringLiteral("remove"), result.detail);
  store->storedClientId(&result);
  report(out, result.notFound, QStringLiteral("nothing is stored after the removal"), result.detail);

  result = store->remove();
  report(out, result.ok, QStringLiteral("a second remove is not an error"), result.detail);

  // whatever happened above, nothing of this run stays in the store
  store->remove();

  out << (g_failed == 0 ? "ALL PASSED" : "SOME STEPS FAILED") << Qt::endl;
  return g_failed == 0 ? 0 : 1;
}
