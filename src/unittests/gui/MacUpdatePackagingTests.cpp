/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacUpdatePackagingTests.h"

#include <QFile>
#include <QRegularExpression>
#include <QTest>

void MacUpdatePackagingTests::cpackDragNDropImage_hasNoLicenseResource()
{
  QFile config(QStringLiteral(DESKFLOW_CPACK_CONFIG));
  QVERIFY2(config.open(QIODevice::ReadOnly), qPrintable(config.errorString()));
  const auto text = QString::fromUtf8(config.readAll());
  const QRegularExpression setting(
      QStringLiteral(
          "set\\(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE\\s+\"?([^\"\\n\\r)]+)\"?\\)"
      )
  );
  const auto match = setting.match(text);
  QVERIFY2(match.hasMatch(), qPrintable(text));
  QCOMPARE(match.captured(1).trimmed(), QStringLiteral("OFF"));
}

QTEST_MAIN(MacUpdatePackagingTests)
