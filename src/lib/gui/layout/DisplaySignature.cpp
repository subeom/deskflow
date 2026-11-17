/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/DisplaySignature.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace deskflow::gui::layout {

namespace {

// what a signature may hold: a position within what the arrangement allows, a size that a display can have
constexpr int kMaxCoordinate = 1000000;
constexpr int kMaxSide = 100000;
constexpr int kMaxScale = 1000;
constexpr int kMaxTextLength = 128;

QString hex(uint32_t value, int width)
{
  return QStringLiteral("%1").arg(value, width, 16, QLatin1Char('0'));
}

// the text of an id may not have the separators of the key in it
QString cleaned(const QString &text)
{
  QString result = text.trimmed().toLower();
  result.remove(QLatin1Char('|'));
  result.remove(QLatin1Char(';'));
  return result.left(kMaxTextLength);
}

// the entry of a display in the key: identity|class|position|main
QString entryOf(const DisplayInfo &d)
{
  return QStringLiteral("%1|%2%3x%4@%5|%6,%7|%8")
      .arg(
          d.identity(), d.builtIn ? QStringLiteral("b") : QStringLiteral("e"), QString::number(d.w),
          QString::number(d.h), QString::number(d.scalePercent), QString::number(d.x), QString::number(d.y),
          d.primary ? QStringLiteral("p") : QStringLiteral("-")
      );
}

// the order of a normalised list: position, size, then what says which display it is
auto orderKey(const DisplayInfo &d)
{
  return std::make_tuple(d.x, d.y, d.w, d.h, d.identity(), d.builtIn, d.scalePercent, d.primary);
}

// of two displays that are the same rectangle (one mirrors the other), which one is counted: the main display, then
// the built in one, then the one with the smaller identity
bool betterOfMirrored(const DisplayInfo &a, const DisplayInfo &b)
{
  if (a.primary != b.primary)
    return a.primary;
  if (a.builtIn != b.builtIn)
    return a.builtIn;
  return a.identity() < b.identity();
}

bool isInteger(const QJsonValue &value, double min, double max, int64_t *out)
{
  if (!value.isDouble())
    return false;
  const double number = value.toDouble();
  if (!std::isfinite(number) || number != std::floor(number) || number < min || number > max)
    return false;
  *out = static_cast<int64_t>(number);
  return true;
}

// a field that may be missing (then it has its default), but not be of another type
template <typename T> bool optionalInteger(const QJsonObject &object, const QString &name, double min, double max, T *out)
{
  const auto value = object.value(name);
  if (value.isUndefined())
    return true;
  int64_t parsed = 0;
  if (!isInteger(value, min, max, &parsed))
    return false;
  *out = static_cast<T>(parsed);
  return true;
}

bool optionalBool(const QJsonObject &object, const QString &name, bool *out)
{
  const auto value = object.value(name);
  if (value.isUndefined())
    return true;
  if (!value.isBool())
    return false;
  *out = value.toBool();
  return true;
}

bool optionalText(const QJsonObject &object, const QString &name, QString *out)
{
  const auto value = object.value(name);
  if (value.isUndefined())
    return true;
  if (!value.isString() || value.toString().size() > kMaxTextLength)
    return false;
  *out = value.toString();
  return true;
}

std::optional<DisplayInfo> displayFromJson(const QJsonObject &object)
{
  DisplayInfo d;
  const double max32 = static_cast<double>(std::numeric_limits<uint32_t>::max());
  if (!optionalText(object, QStringLiteral("uuid"), &d.uuid) ||
      !optionalInteger(object, QStringLiteral("vendor"), 0, max32, &d.vendor) ||
      !optionalInteger(object, QStringLiteral("model"), 0, max32, &d.model) ||
      !optionalInteger(object, QStringLiteral("serial"), 0, max32, &d.serial) ||
      !optionalBool(object, QStringLiteral("builtIn"), &d.builtIn) ||
      !optionalBool(object, QStringLiteral("primary"), &d.primary) ||
      !optionalInteger(object, QStringLiteral("x"), -kMaxCoordinate, kMaxCoordinate, &d.x) ||
      !optionalInteger(object, QStringLiteral("y"), -kMaxCoordinate, kMaxCoordinate, &d.y) ||
      !optionalInteger(object, QStringLiteral("scale"), 0, kMaxScale, &d.scalePercent) ||
      !optionalText(object, QStringLiteral("label"), &d.label))
    return std::nullopt;
  // a size is not optional
  int64_t w = 0;
  int64_t h = 0;
  if (!isInteger(object.value(QStringLiteral("w")), 1, kMaxSide, &w) ||
      !isInteger(object.value(QStringLiteral("h")), 1, kMaxSide, &h))
    return std::nullopt;
  d.w = static_cast<int>(w);
  d.h = static_cast<int>(h);
  return d;
}

} // namespace

QString DisplayInfo::identity() const
{
  if (!uuid.trimmed().isEmpty())
    return QStringLiteral("u:") + cleaned(uuid);
  if (vendor != 0 || model != 0 || serial != 0)
    return QStringLiteral("v:%1:%2:%3").arg(hex(vendor, 4), hex(model, 4), QString::number(serial, 16));
  return {};
}

bool DisplayInfo::sameClassAs(const DisplayInfo &other) const
{
  return builtIn == other.builtIn && w == other.w && h == other.h && scalePercent == other.scalePercent;
}

DisplaySignature DisplaySignature::fromDisplays(const std::vector<DisplayInfo> &displays)
{
  DisplaySignature signature;

  std::vector<DisplayInfo> valid;
  for (const auto &display : displays) {
    if (display.w > 0 && display.h > 0)
      valid.push_back(display);
  }

  // a display that is the same rectangle as another is a mirror of it: one is counted
  std::vector<DisplayInfo> distinct;
  for (const auto &display : valid) {
    const auto same = std::ranges::find_if(distinct, [&](const DisplayInfo &other) {
      return other.x == display.x && other.y == display.y && other.w == display.w && other.h == display.h;
    });
    if (same == distinct.end())
      distinct.push_back(display);
    else if (betterOfMirrored(display, *same))
      *same = display;
  }

  std::ranges::sort(distinct, [](const DisplayInfo &a, const DisplayInfo &b) { return orderKey(a) < orderKey(b); });
  if (distinct.size() > static_cast<size_t>(kMaxDisplays)) {
    distinct.resize(kMaxDisplays);
    signature.m_truncated = true;
  }
  signature.m_displays = std::move(distinct);
  return signature;
}

QString DisplaySignature::key() const
{
  QStringList entries;
  for (const auto &display : m_displays)
    entries << entryOf(display);
  return entries.join(QLatin1Char(';'));
}

QString DisplaySignature::id() const
{
  if (m_displays.empty())
    return {};
  return QString::fromLatin1(QCryptographicHash::hash(key().toUtf8(), QCryptographicHash::Sha256).toHex().left(8));
}

QString DisplaySignature::summary() const
{
  QStringList parts;
  for (const auto &d : m_displays) {
    auto text = QStringLiteral("%1 %2x%3@%4 at %5,%6")
                    .arg(
                        d.builtIn ? QStringLiteral("built-in") : QStringLiteral("external"), QString::number(d.w),
                        QString::number(d.h),
                        d.scalePercent > 0 ? QString::number(d.scalePercent) + QLatin1Char('%') : QStringLiteral("?"),
                        QString::number(d.x), QString::number(d.y)
                    );
    if (d.primary)
      text += QStringLiteral(" (main)");
    // the maker and the model say which monitor it is, the serial number is not shown
    if (d.vendor != 0 || d.model != 0)
      text += QStringLiteral(" [%1:%2]").arg(hex(d.vendor, 4), hex(d.model, 4));
    parts << text;
  }
  return parts.join(QStringLiteral(" + "));
}

std::vector<deskflow::server::MonitorRect> DisplaySignature::monitors() const
{
  std::vector<deskflow::server::MonitorRect> rects;
  for (const auto &d : m_displays)
    rects.push_back({d.x, d.y, d.w, d.h});
  return rects;
}

QJsonObject DisplaySignature::toJson() const
{
  QJsonArray list;
  for (const auto &d : m_displays) {
    QJsonObject object;
    if (!d.uuid.isEmpty())
      object.insert(QStringLiteral("uuid"), d.uuid);
    object.insert(QStringLiteral("vendor"), static_cast<double>(d.vendor));
    object.insert(QStringLiteral("model"), static_cast<double>(d.model));
    object.insert(QStringLiteral("serial"), static_cast<double>(d.serial));
    object.insert(QStringLiteral("builtIn"), d.builtIn);
    object.insert(QStringLiteral("primary"), d.primary);
    object.insert(QStringLiteral("x"), d.x);
    object.insert(QStringLiteral("y"), d.y);
    object.insert(QStringLiteral("w"), d.w);
    object.insert(QStringLiteral("h"), d.h);
    object.insert(QStringLiteral("scale"), d.scalePercent);
    if (!d.label.isEmpty())
      object.insert(QStringLiteral("label"), d.label);
    list.append(object);
  }
  return QJsonObject{{QStringLiteral("displays"), list}};
}

std::optional<DisplaySignature> DisplaySignature::fromJson(const QJsonObject &object)
{
  const auto list = object.value(QStringLiteral("displays"));
  if (!list.isArray())
    return std::nullopt;
  const auto array = list.toArray();
  if (array.isEmpty() || array.size() > kMaxDisplays)
    return std::nullopt;

  std::vector<DisplayInfo> displays;
  for (const auto &entry : array) {
    if (!entry.isObject())
      return std::nullopt;
    const auto display = displayFromJson(entry.toObject());
    if (!display)
      return std::nullopt;
    displays.push_back(*display);
  }
  auto signature = fromDisplays(displays);
  if (!signature.isValid())
    return std::nullopt;
  return signature;
}

} // namespace deskflow::gui::layout
