/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlTexts.h"

#include <QCoreApplication>

namespace deskflow::awdl {

QString formatMs(double milliseconds)
{
  return milliseconds < 9.95 ? QString::number(milliseconds, 'f', 1) : QString::number(milliseconds, 'f', 0);
}

QString stateText(Summary summary)
{
  switch (summary) {
  case Summary::NotAvailable:
    return QCoreApplication::translate("AwdlTexts", "Not available on this Mac.");
  case Summary::Unknown:
    return QCoreApplication::translate("AwdlTexts", "Not read yet.");
  case Summary::Off:
    return QCoreApplication::translate(
        "AwdlTexts", "Off. AirDrop and Handoff cannot use it until it is turned back on."
    );
  case Summary::OnWired:
    return QCoreApplication::translate(
        "AwdlTexts", "On. This computer is on a wired network, so the wireless link does not matter."
    );
  case Summary::OnNotWiFi:
    return QCoreApplication::translate(
        "AwdlTexts", "On. The connection of this computer is not Wi-Fi, so the delay is not measured."
    );
  case Summary::OnNotMeasured:
    return QCoreApplication::translate("AwdlTexts", "On. The delay to the router has not been measured yet.");
  case Summary::OnNoData:
    return QCoreApplication::translate("AwdlTexts", "On. The router did not answer enough probes to tell.");
  case Summary::OnClean:
    return QCoreApplication::translate("AwdlTexts", "On, no latency problem seen.");
  case Summary::OnStutter:
    return QCoreApplication::translate(
        "AwdlTexts", "On, and the delay to the router has spikes: the mouse can stutter."
    );
  }
  return {};
}

QString measurementText(const BurstStats &stats)
{
  if (stats.pattern == Pattern::NotMeasured || stats.received <= 0)
    return {};
  return QCoreApplication::translate("AwdlTexts", "Last measurement: median %1 ms, worst %2 ms.")
      .arg(formatMs(stats.medianMs), formatMs(stats.worstMs));
}

QString bannerText(const BurstStats &stats)
{
  if (stats.received <= 0) {
    return QCoreApplication::translate(
        "AwdlTexts", "The wireless link of AirDrop and Handoff is on and the delay to the router has spikes: the mouse "
                     "can stutter."
    );
  }
  return QCoreApplication::translate(
             "AwdlTexts", "The wireless link of AirDrop and Handoff is on and the delay to the router has spikes "
                          "(median %1 ms, worst %2 ms): the mouse can stutter."
  )
      .arg(formatMs(stats.medianMs), formatMs(stats.worstMs));
}

QString prompt(Direction direction)
{
  if (direction == Direction::Down) {
    return QCoreApplication::translate(
        "AwdlTexts", "Deskflow turns off the wireless link (AWDL) of AirDrop and Handoff, so that the mouse does not "
                     "stutter while sharing. AirDrop, Handoff and Universal Control stop until it is turned back on."
    );
  }
  return QCoreApplication::translate(
      "AwdlTexts", "Deskflow turns the wireless link (AWDL) of AirDrop and Handoff back on."
  );
}

} // namespace deskflow::awdl
