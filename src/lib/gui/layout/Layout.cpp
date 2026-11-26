/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/Layout.h"

#include "server/Arrangement.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace deskflow::gui::layout {

namespace {

constexpr int kMaxNameLength = 128;
// places within a millionth of a unit are the same place
constexpr double kSamePlace = 1e-6;

bool nearlyEqual(double a, double b)
{
  return std::abs(a - b) <= kSamePlace;
}

bool number(const QJsonValue &value, double min, double max, double *out)
{
  if (!value.isDouble())
    return false;
  const double v = value.toDouble();
  if (!std::isfinite(v) || v < min || v > max)
    return false;
  *out = v;
  return true;
}

std::optional<PlacedComputer> computerFromJson(const QJsonObject &object)
{
  PlacedComputer computer;
  const auto name = object.value(QStringLiteral("name"));
  if (!name.isString() || name.toString().trimmed().isEmpty() || name.toString().size() > kMaxNameLength)
    return std::nullopt;
  computer.name = name.toString();

  const auto server = object.value(QStringLiteral("server"));
  if (!server.isUndefined()) {
    if (!server.isBool())
      return std::nullopt;
    computer.isServer = server.toBool();
  }

  using deskflow::server::arrangement::kMaxCoordinate;
  using deskflow::server::arrangement::kMaxZoom;
  using deskflow::server::arrangement::kMinZoom;
  if (!number(object.value(QStringLiteral("x")), -kMaxCoordinate, kMaxCoordinate, &computer.x) ||
      !number(object.value(QStringLiteral("y")), -kMaxCoordinate, kMaxCoordinate, &computer.y))
    return std::nullopt;
  const auto zoom = object.value(QStringLiteral("zoom"));
  if (!zoom.isUndefined() && !number(zoom, kMinZoom, kMaxZoom, &computer.zoom))
    return std::nullopt;
  return computer;
}

} // namespace

const PlacedComputer *Layout::find(const QString &name) const
{
  if (name.isEmpty())
    return nullptr;
  const auto found = std::ranges::find_if(computers, [&](const PlacedComputer &c) {
    return c.name.compare(name, Qt::CaseInsensitive) == 0;
  });
  return found == computers.end() ? nullptr : &*found;
}

bool Layout::sameAs(const Layout &other) const
{
  if (computers.size() != other.computers.size())
    return false;
  for (const auto &mine : computers) {
    const auto *theirs = other.find(mine.name);
    if (theirs == nullptr || theirs->isServer != mine.isServer || !nearlyEqual(theirs->x, mine.x) ||
        !nearlyEqual(theirs->y, mine.y) || !nearlyEqual(theirs->zoom, mine.zoom))
      return false;
  }
  return true;
}

QJsonObject Layout::toJson() const
{
  QJsonArray list;
  for (const auto &computer : computers) {
    list.append(QJsonObject{
        {QStringLiteral("name"), computer.name},
        {QStringLiteral("server"), computer.isServer},
        {QStringLiteral("x"), computer.x},
        {QStringLiteral("y"), computer.y},
        {QStringLiteral("zoom"), computer.zoom},
    });
  }
  return QJsonObject{{QStringLiteral("computers"), list}};
}

std::optional<Layout> Layout::fromJson(const QJsonObject &object)
{
  const auto list = object.value(QStringLiteral("computers"));
  if (!list.isArray())
    return std::nullopt;
  const auto array = list.toArray();
  if (array.isEmpty() || array.size() > kMaxComputers)
    return std::nullopt;

  Layout layout;
  for (const auto &entry : array) {
    if (!entry.isObject())
      return std::nullopt;
    const auto computer = computerFromJson(entry.toObject());
    if (!computer || layout.find(computer->name) != nullptr)
      return std::nullopt;
    layout.computers.push_back(*computer);
  }
  return layout;
}

} // namespace deskflow::gui::layout
