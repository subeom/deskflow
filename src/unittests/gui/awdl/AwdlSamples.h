/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/awdl/AwdlModel.h"

#include <QList>
#include <QMap>

// What a test says that the network did: a route and the answers of a burst of probes
namespace awdltest {

using namespace deskflow::awdl;

inline Route routeOf(
    LinkKind kind, const QString &gateway = QStringLiteral("192.168.0.1"), const QString &name = QStringLiteral("en0")
)
{
  Route route;
  route.interfaceName = name;
  route.gateway = gateway;
  route.kind = kind;
  return route;
}

// A burst of 40 probes about 3 ms, with the given probes at the given latency
inline QList<Sample> burstOf(const QMap<int, double> &spikes = {})
{
  QList<Sample> samples;
  for (int i = 0; i < kBurstCount; ++i) {
    Sample sample;
    sample.sequence = i;
    sample.rttMs = spikes.contains(i) ? spikes.value(i) : 2.0 + 0.3 * ((i * 3) % 7);
    samples.append(sample);
  }
  return samples;
}

// what AWDL does: spikes of 25 to 70 ms, 3.6 s apart
inline QList<Sample> stutter()
{
  return burstOf({{4, 31.0}, {5, 64.0}, {22, 48.0}, {23, 25.5}});
}

inline QList<Sample> clean()
{
  return burstOf();
}

} // namespace awdltest
