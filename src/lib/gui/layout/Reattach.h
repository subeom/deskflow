/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/Arrangement.h"
#include "server/MonitorGeometry.h"

#include <string>
#include <vector>

namespace deskflow::gui::layout {

//! What putting the computers against the server again, after its displays changed, did
struct ReattachResult
{
  //! The arrangement with the displays of the server that were given, and the computers that were moved
  deskflow::server::arrangement::Arrangement arrangement;
  //! The computers that touched the server before and were put against it again
  std::vector<std::string> reattached;
  //! Every computer that was moved: those above, and the computers that touch them, which move with them
  std::vector<std::string> moved;
  //! The computers that touched the server before, touch it no more, and could not be put against it: they stay
  std::vector<std::string> notReattached;

  bool changed() const
  {
    return !moved.empty();
  }
};

//! The fallback for displays that nobody remembered: keep the computers that were next to the server next to it
/*!
\p before is the arrangement as it was, with the displays that the server had. \p newServerMonitors are the displays it
has now. A group of computers (computers that touch each other) that touched the server before and touch it no more
is moved, as it is, until its computer with the longest contact is against the same side of the box of the new
displays, at the same fraction of that side; when that overlaps something it slides along the side until it does not.
The result is checked: the moved group has to touch the server and not overlap anything, else it stays where it was and
is reported. A group that still touches the server is never moved, and neither is one that never touched it.
*/
ReattachResult reattachToServer(
    const deskflow::server::arrangement::Arrangement &before,
    const std::vector<deskflow::server::MonitorRect> &newServerMonitors
);

} // namespace deskflow::gui::layout
