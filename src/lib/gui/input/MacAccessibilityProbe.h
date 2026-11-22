/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorPolicy.h"

#include <QObject>

#include <cstdint>
#include <optional>
#include <vector>

namespace deskflow::gui::input {

struct MacAccessibilitySnapshot
{
  EditableFocusState state = EditableFocusState::Unknown;
  int64_t applicationPid = 0;
};

std::vector<int64_t> macAccessibilityApplicationPidCandidates(
    std::optional<int64_t> systemWideApplicationPid,
    std::optional<int64_t> frontmostApplicationPid
);

class MacAccessibilityProbe : public QObject
{
  Q_OBJECT

public:
  explicit MacAccessibilityProbe(QObject *parent = nullptr);

  void request(
      uint64_t generation,
      std::optional<int64_t> frontmostApplicationPid
  );

Q_SIGNALS:
  void completed(uint64_t generation, const MacAccessibilitySnapshot &snapshot);

private:
  void finish(uint64_t generation, const MacAccessibilitySnapshot &snapshot);

  uint64_t m_requestedGeneration = 0;
  std::optional<int64_t> m_requestedFrontmostApplicationPid;
  bool m_running = false;
};

} // namespace deskflow::gui::input
