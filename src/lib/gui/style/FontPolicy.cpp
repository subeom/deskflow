/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FontPolicy.h"

#include <algorithm>

namespace deskflow::gui::style {

namespace {

QList<int> s_weights;

bool isKorean(const QString &language)
{
  return language == QLatin1String("ko") || language.startsWith(QLatin1String("ko_"));
}

bool isSimplifiedChinese(const QString &language)
{
  return language == QLatin1String("zh_CN") || language.startsWith(QLatin1String("zh_CN"));
}

} // namespace

QStringList parseFamilyList(const QString &text)
{
  QStringList families;
  const auto parts = text.split(QLatin1Char(','));
  for (auto part : parts) {
    part = part.trimmed();
    if (part.size() >= 2 && ((part.startsWith(QLatin1Char('"')) && part.endsWith(QLatin1Char('"'))) ||
                             (part.startsWith(QLatin1Char('\'')) && part.endsWith(QLatin1Char('\''))))) {
      part = part.mid(1, part.size() - 2).trimmed();
    }
    if (!part.isEmpty() && !families.contains(part, Qt::CaseInsensitive))
      families << part;
  }
  return families;
}

QStringList languageFontCandidates(TextPlatform platform, const QString &language)
{
  if (platform == TextPlatform::Windows) {
    if (isKorean(language))
      return {QStringLiteral("Malgun Gothic")};
    if (isSimplifiedChinese(language))
      return {QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei")};
  } else if (platform == TextPlatform::Mac) {
    if (isKorean(language))
      return {QStringLiteral("Apple SD Gothic Neo")};
  }
  return {};
}

FontChoice chooseFonts(const FontPolicyInput &input)
{
  FontChoice choice;
  const auto fromVariable = parseFamilyList(input.variable);
  if (!fromVariable.isEmpty()) {
    choice.source = FontChoice::Source::Variable;
    choice.families = fromVariable;
  } else if (input.hasFamily) {
    for (const auto &candidate : languageFontCandidates(input.platform, input.language)) {
      if (!input.hasFamily(candidate))
        continue;
      choice.source = FontChoice::Source::Language;
      choice.families =
          input.systemFamily.isEmpty() ? QStringList{candidate} : QStringList{input.systemFamily, candidate};
      break;
    }
  }
  if (!choice.families.isEmpty() && input.weightsOf)
    choice.weights = input.weightsOf(choice.families.last());
  return choice;
}

int snapWeight(int requested, const QList<int> &available)
{
  if (available.isEmpty() || available.contains(requested))
    return requested;
  auto sorted = available;
  std::sort(sorted.begin(), sorted.end());
  const auto heavier = [&](int from, int to) -> int {
    for (const int weight : sorted) {
      if (weight > from && weight <= to)
        return weight;
    }
    return -1;
  };
  const auto lighter = [&]() -> int {
    for (auto it = sorted.crbegin(); it != sorted.crend(); ++it) {
      if (*it < requested)
        return *it;
    }
    return -1;
  };
  if (requested >= 400 && requested <= 500) {
    // between what was asked for and 500 first, then lighter, then heavier
    if (const int found = heavier(requested, 500); found > 0)
      return found;
    if (const int found = lighter(); found > 0)
      return found;
    return heavier(500, 1000);
  }
  if (requested > 500) {
    if (const int found = heavier(requested, 1000); found > 0)
      return found;
    return lighter();
  }
  if (const int found = lighter(); found > 0)
    return found;
  return heavier(requested, 1000);
}

void setFontWeights(const QList<int> &weights)
{
  s_weights = weights;
}

QList<int> fontWeights()
{
  return s_weights;
}

QFont::Weight fontWeight(QFont::Weight requested)
{
  return s_weights.isEmpty() ? requested : static_cast<QFont::Weight>(snapWeight(requested, s_weights));
}

} // namespace deskflow::gui::style
