/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTranslator>

// The Korean translation as it is shipped: the .qm that the build makes of translations/deskflow_ko.ts (the build of a
// test that uses this header makes it first). The tests that put the application in Korean install it, so that what
// they look at is what a person with a Korean system sees, and not a stand-in of a few strings.
namespace koreantest {

inline QString shippedQm()
{
  return QStringLiteral(DESKFLOW_KO_QM);
}

/*!
 * Takes the place of the translation that is shipped and asks it: the words that it does not have are kept, with the
 * context that asked for them, so that a test can say by name what a screen shows in English. Qt's own texts (the
 * buttons of its dialogs and so on) are not the program's: their contexts begin with Q (the program's own context
 * QObject is not one of them), and the translations of Qt are not loaded by a test.
 */
class RecordingTranslator : public QTranslator
{
public:
  explicit RecordingTranslator(QObject *parent = nullptr) : QTranslator(parent)
  {
  }

  bool loadShipped()
  {
    return m_shipped.load(shippedQm());
  }

  QString translate(
      const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1
  ) const override
  {
    const QString found = m_shipped.translate(context, sourceText, disambiguation, n);
    if (found.isNull() && context && sourceText)
      m_missing.insert(describe(QString::fromUtf8(context), QString::fromUtf8(sourceText)));
    return found;
  }

  bool isEmpty() const override
  {
    return false;
  }

  //! "Context | English", sorted, of what was asked for and is not translated
  QStringList missing() const
  {
    QStringList names;
    for (const auto &name : std::as_const(m_missing)) {
      if (isTheProgram(name))
        names << name;
    }
    names.sort();
    return names;
  }

  void forget()
  {
    m_missing.clear();
  }

private:
  static QString describe(const QString &context, const QString &source)
  {
    return QStringLiteral("%1 | %2").arg(context, source);
  }

  static bool isTheProgram(const QString &name)
  {
    const auto context = name.left(name.indexOf(QStringLiteral(" | ")));
    if (context.startsWith(QLatin1Char('Q')) && context != QStringLiteral("QObject"))
      return false;
    // a text that has no letter in it (a number and a unit, a separator) is the same in every language
    const auto source = name.mid(name.indexOf(QStringLiteral(" | ")) + 3);
    return source.contains(QRegularExpression(QStringLiteral("[A-Za-z]")));
  }

  QTranslator m_shipped;
  mutable QSet<QString> m_missing;
};

} // namespace koreantest
