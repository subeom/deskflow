/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/ServerConfigLayout.h"

#include "gui/layout/Reattach.h"

#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace deskflow::gui::layout {

namespace {

using namespace deskflow::server::arrangement;

// what is between a computer that is put aside and the others (the same as the canvas puts between them)
constexpr double kGapOfLeftovers = 200.0;

Layout layoutOf(const Arrangement &arrangement)
{
  Layout layout;
  for (const auto &computer : arrangement.computers)
    layout.computers.push_back(
        {QString::fromStdString(computer.name), computer.isServer, computer.originX, computer.originY, computer.zoom}
    );
  return layout;
}

std::optional<Issue> firstError(const Arrangement &arrangement)
{
  for (const auto &issue : validate(arrangement)) {
    if (issue.severity() == Issue::Severity::Error)
      return issue;
  }
  return std::nullopt;
}

// the same rectangles, whatever the order and however often a rectangle is in the list (a mirror)
bool sameRectangles(std::vector<deskflow::server::MonitorRect> a, std::vector<deskflow::server::MonitorRect> b)
{
  const auto less = [](const deskflow::server::MonitorRect &l, const deskflow::server::MonitorRect &r) {
    return std::tie(l.x, l.y, l.w, l.h) < std::tie(r.x, r.y, r.w, r.h);
  };
  for (auto *list : {&a, &b}) {
    std::ranges::sort(*list, less);
    list->erase(std::unique(list->begin(), list->end()), list->end());
  }
  return a == b;
}

} // namespace

Layout captureLayout(const ServerConfig &config)
{
  if (!config.hasArrangement())
    return {};
  return layoutOf(config.arrangement());
}

ApplyOutcome applyLayout(ServerConfig &config, const Layout &layout)
{
  ApplyOutcome outcome;
  if (!config.hasArrangement()) {
    outcome.error = QStringLiteral("there is no arrangement");
    return outcome;
  }

  auto arranged = config.arrangement();
  const auto before = layoutOf(arranged);

  std::vector<size_t> unknown;
  for (size_t i = 0; i < arranged.computers.size(); ++i) {
    auto &computer = arranged.computers[i];
    const auto name = QString::fromStdString(computer.name);
    if (const auto *placed = layout.find(name)) {
      computer.originX = placed->x;
      computer.originY = placed->y;
      if (!computer.isServer)
        computer.zoom = placed->zoom;
    } else {
      outcome.notInLayout << name;
      unknown.push_back(i);
    }
  }
  // nobody of the configuration is in the layout: there is nothing to put back
  if (unknown.size() == arranged.computers.size()) {
    outcome.notInLayout.clear();
    outcome.applied = true;
    return outcome;
  }

  // a computer that the layout does not know stays where it is, unless that overlaps one that was put: then it is
  // put beside the others, as a computer that has no place is
  if (firstError(arranged) && !unknown.empty()) {
    Rect bounds;
    bool any = false;
    for (size_t i = 0; i < arranged.computers.size(); ++i) {
      if (std::ranges::find(unknown, i) != unknown.end())
        continue;
      const auto box = arranged.computers[i].box();
      bounds = any ? Rect{std::min(bounds.x, box.x), std::min(bounds.y, box.y),
                          std::max(bounds.right(), box.right()) - std::min(bounds.x, box.x),
                          std::max(bounds.bottom(), box.bottom()) - std::min(bounds.y, box.y)}
                   : box;
      any = true;
    }
    for (const auto i : unknown) {
      auto &computer = arranged.computers[i];
      const auto shape = computer.shape();
      computer.originX = bounds.right() + kGapOfLeftovers - computer.zoom * shape.x;
      computer.originY = bounds.y - computer.zoom * shape.y;
      const auto box = computer.box();
      bounds.w = box.right() - bounds.x;
      bounds.h = std::max(bounds.bottom(), box.bottom()) - bounds.y;
    }
  }
  if (const auto error = firstError(arranged)) {
    outcome.notInLayout.clear();
    outcome.error = QStringLiteral("the arrangement would have an error (%1 and %2)")
                        .arg(QString::fromStdString(error->computer), QString::fromStdString(error->other));
    return outcome;
  }

  // it is good: now it is made, as a whole
  const auto wanted = layoutOf(arranged);
  outcome.applied = true;
  outcome.changed = !wanted.sameAs(before);
  if (!outcome.changed)
    return outcome;

  for (const auto &computer : arranged.computers) {
    if (computer.isServer)
      continue;
    const auto name = QString::fromStdString(computer.name);
    const auto *was = before.find(name);
    if (was != nullptr && std::abs(was->zoom - computer.zoom) > 1e-9)
      config.setComputerZoom(name, computer.zoom);
  }
  config.setArrangement(arranged);
  return outcome;
}

ReattachOutcome reattachLayout(const ServerConfig &config, const DisplaySignature &from, const DisplaySignature &to)
{
  ReattachOutcome outcome;
  if (!config.hasArrangement() || !from.isValid() || !to.isValid())
    return outcome;
  // The computers are put against the displays that the arrangement is made with: when the configuration takes the
  // displays of the server from somewhere that is not the displays of the signature (a platform that tells one rectangle
  // for the whole screen), they would be put against a screen that is not there.
  if (!sameRectangles(ServerConfig::serverMonitors(), to.monitors()))
    return outcome;

  const auto before = config.arrangementWith(from.monitors());
  const auto result = reattachToServer(before, to.monitors());
  for (const auto &name : result.notReattached)
    outcome.notReattached << QString::fromStdString(name);
  if (!result.changed())
    return outcome;
  for (const auto &name : result.reattached)
    outcome.reattached << QString::fromStdString(name);
  outcome.layout = layoutOf(result.arrangement);
  return outcome;
}

QString generatedConfigText(const ServerConfig &config)
{
  QString text;
  QTextStream stream(&text);
  stream << config;
  stream.flush();
  return text;
}

} // namespace deskflow::gui::layout
