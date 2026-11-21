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

QString permissionText(PermissionState state, bool filePresent)
{
  switch (state) {
  case PermissionState::Allowed:
    return QCoreApplication::translate(
        "AwdlTexts", "Allowed. Deskflow turns the link off and on without asking for a password."
    );
  case PermissionState::NotAllowed:
    if (filePresent) {
      return QCoreApplication::translate(
          "AwdlTexts", "Not allowed. The file /etc/sudoers.d/deskflow-awdl is there, but sudo does not use it for "
                       "this. Deskflow asks for the administrator password."
      );
    }
    return QCoreApplication::translate("AwdlTexts", "Not allowed. Deskflow asks for the administrator password.");
  case PermissionState::Unknown:
    break;
  }
  return QCoreApplication::translate("AwdlTexts", "Unknown. Deskflow asks for the administrator password.");
}

QString permissionNoteText(PermissionNote note)
{
  switch (note) {
  case PermissionNote::Added:
    return QCoreApplication::translate("AwdlTexts", "The file was added.");
  case PermissionNote::Removed:
    return QCoreApplication::translate("AwdlTexts", "The file was removed.");
  case PermissionNote::Cancelled:
    return QCoreApplication::translate("AwdlTexts", "The password was not given. Nothing was changed.");
  case PermissionNote::Failed:
    return QCoreApplication::translate("AwdlTexts", "It did not work. Nothing was changed.");
  case PermissionNote::Refused:
    return QCoreApplication::translate(
        "AwdlTexts", "The name of this account cannot be used in the file, so nothing was changed."
    );
  case PermissionNote::None:
    break;
  }
  return {};
}

QString confirmAllowTitle()
{
  return QCoreApplication::translate("AwdlTexts", "Allow without a password?");
}

QString confirmAllowText()
{
  return QCoreApplication::translate(
      "AwdlTexts",
      "Deskflow asks for the administrator password once and adds one small file, "
      "/etc/sudoers.d/deskflow-awdl. The file lets your account run only \"ifconfig awdl0 down\" and "
      "\"ifconfig awdl0 up\" as root without a password.\n\nAny program that runs as you can then do the same, "
      "which stops or starts AirDrop and Handoff. You can remove it here with \"Remove permission\", or by deleting "
      "the file."
  );
}

QString permissionPrompt(PermissionAction action)
{
  if (action == PermissionAction::Allow) {
    return QCoreApplication::translate(
        "AwdlTexts", "Deskflow adds the file /etc/sudoers.d/deskflow-awdl, so that your account can turn the "
                     "wireless link (AWDL) off and on without a password."
    );
  }
  return QCoreApplication::translate("AwdlTexts", "Deskflow removes the file /etc/sudoers.d/deskflow-awdl.");
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
