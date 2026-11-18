/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TextSizeModel.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace deskflow::gui::style {

namespace {

constexpr int kLeastPercentStepIndex = 0;

double roundedToHundredths(double value)
{
  return std::round(value * 100.0) / 100.0;
}

} // namespace

double minimumBodyPixels(TextPlatform platform)
{
  switch (platform) {
  case TextPlatform::Windows:
    return 14.0;
  case TextPlatform::Mac:
  case TextPlatform::Linux:
  case TextPlatform::Other:
    break;
  }
  return 13.0;
}

QList<int> textSizePercentSteps()
{
  return {90, 100, 115, 130, 150};
}

int snapTextPercent(int percent)
{
  return textSizePercentSteps().contains(percent) ? percent : kDefaultTextPercent;
}

TextSizePlan planTextSize(const TextSizeInput &input)
{
  // an OS that says less than 1 does not make text smaller than its font
  const auto base = qMax(0.0, input.systemPixels) * qMax(1.0, input.osTextScale);
  const auto floor = minimumBodyPixels(input.platform);

  TextSizePlan plan;
  plan.raisedByFloor = base < floor;
  const auto percent = snapTextPercent(input.percent);
  plan.bodyPixels =
      roundedToHundredths(std::clamp(qMax(base, floor) * percent / 100.0, kLeastBodyPixels, kMostBodyPixels));
  plan.metricScale = std::min(kMostMetricScale, std::max(1.0, plan.bodyPixels / kReferenceBodyPixels));
  return plan;
}

int stepTextPercent(int percent, int direction)
{
  if (direction == 0)
    return kDefaultTextPercent;
  const auto steps = textSizePercentSteps();
  const auto at = steps.indexOf(snapTextPercent(percent));
  const auto next = std::clamp<qsizetype>(at + (direction > 0 ? 1 : -1), kLeastPercentStepIndex, steps.count() - 1);
  return steps.at(next);
}

double osTextScaleFromPercent(int percent)
{
  if (percent < 100 || percent > 225)
    return 1.0;
  return percent / 100.0;
}

int scaledPixels(int logicalPixels, double scale)
{
  const auto scaled = static_cast<int>(std::lround(logicalPixels * scale));
  // a size that is there stays there, however small
  return logicalPixels > 0 ? std::max(1, scaled) : scaled;
}

} // namespace deskflow::gui::style
