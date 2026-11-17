/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>
#include <vector>

namespace deskflow::gui::layout {

//! Where one computer is in the arrangement (experimental: layout profiles)
struct PlacedComputer
{
  QString name;
  bool isServer = false;
  //! Where the origin of the coordinates of the computer is in the arrangement
  double x = 0;
  double y = 0;
  //! Arrangement units per unit of the computer
  double zoom = 1.0;

  bool operator==(const PlacedComputer &) const = default;
};

//! The arrangement of the computers, as far as a change of the displays of the server changes it
/*!
Only where each computer is and how big it is drawn: the monitors of a computer, its size and its display scales say
what the computer is, not where it is, and a change of the displays of the server does not change them.
*/
struct Layout
{
  static constexpr int kMaxComputers = 32;

  std::vector<PlacedComputer> computers;

  bool isEmpty() const
  {
    return computers.empty();
  }
  //! The computer with this name (the case does not matter), or nothing
  const PlacedComputer *find(const QString &name) const;

  //! The same computers in the same places, whatever the order of the list; places within a millionth are the same
  bool sameAs(const Layout &other) const;

  QJsonObject toJson() const;
  //! Only a layout that has valid names (not empty, not twice), places and zooms, and is bounded; else nothing
  static std::optional<Layout> fromJson(const QJsonObject &object);
};

} // namespace deskflow::gui::layout
