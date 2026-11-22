/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorPolicy.h"

#include <QObject>

#include <cstdint>

namespace deskflow::gui::input {

struct MacAccessibilitySnapshot
{
  EditableFocusState state = EditableFocusState::Unknown;
  int64_t applicationPid = 0;
};

class MacAccessibilityProbe : public QObject
{
  Q_OBJECT

public:
  explicit MacAccessibilityProbe(QObject *parent = nullptr);

  void request(uint64_t generation);

Q_SIGNALS:
  void completed(uint64_t generation, const MacAccessibilitySnapshot &snapshot);

private:
  void finish(uint64_t generation, const MacAccessibilitySnapshot &snapshot);

  uint64_t m_requestedGeneration = 0;
  bool m_running = false;
};

} // namespace deskflow::gui::input
