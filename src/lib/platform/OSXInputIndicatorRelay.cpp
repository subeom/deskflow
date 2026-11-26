/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXInputIndicatorRelay.h"

#include <mutex>
#include <optional>
#include <utility>

namespace deskflow::platform {

struct OSXInputIndicatorRelay::State
{
  State(Scheduler scheduler, Sender sender) : schedule(std::move(scheduler)), send(std::move(sender))
  {
    subscriptionEpoch = deskflow::input::inputIndicatorSubscriptionEpoch();
  }

  std::mutex mutex;
  Scheduler schedule;
  Sender send;
  std::optional<deskflow::input::InputIndicatorActivity> pending;
  uint64_t nextSequence = 1;
  uint64_t generation = 0;
  bool scheduled = false;
  bool closed = false;
  uint64_t subscriptionEpoch = 0;
};

OSXInputIndicatorRelay::OSXInputIndicatorRelay(Scheduler schedule, Sender send)
    : m_state(std::make_shared<State>(std::move(schedule), std::move(send)))
{
}

OSXInputIndicatorRelay::~OSXInputIndicatorRelay()
{
  std::lock_guard<std::mutex> lock(m_state->mutex);
  ++m_state->generation;
  m_state->pending.reset();
  m_state->scheduled = false;
  m_state->closed = true;
}

void OSXInputIndicatorRelay::notify(
    deskflow::input::InputIndicatorActivityKind kind, uint64_t eventTimestampMach, bool isPrimary, bool isOnComputer
)
{
  if (!isPrimary || !isOnComputer || eventTimestampMach == 0 || !deskflow::input::isInputIndicatorSubscribed()) {
    invalidate();
    return;
  }

  bool dispatch = false;
  uint64_t generation = 0;
  uint64_t subscriptionEpoch = deskflow::input::inputIndicatorSubscriptionEpoch();
  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    if (m_state->closed) {
      return;
    }
    if (m_state->subscriptionEpoch != subscriptionEpoch) {
      ++m_state->generation;
      m_state->pending.reset();
      m_state->scheduled = false;
      m_state->subscriptionEpoch = subscriptionEpoch;
    }
    m_state->pending = {
        .sequence = m_state->nextSequence++,
        .eventTimestampMach = eventTimestampMach,
        .kind = kind,
    };
    if (!m_state->scheduled) {
      m_state->scheduled = true;
      generation = m_state->generation;
      dispatch = true;
    }
  }

  if (dispatch) {
    auto state = m_state;
    state->schedule([state, generation, subscriptionEpoch] {
      std::optional<deskflow::input::InputIndicatorActivity> activity;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->closed ||
            state->generation != generation ||
            state->subscriptionEpoch != deskflow::input::inputIndicatorSubscriptionEpoch()) {
          return;
        }

        state->scheduled = false;
        if (state->pending && deskflow::input::isInputIndicatorSubscribed()) {
          activity = *state->pending;
        }
        state->pending.reset();
      }

      if (activity) {
        state->send(*activity);
      }
    });
  }
}

void OSXInputIndicatorRelay::invalidate()
{
  std::lock_guard<std::mutex> lock(m_state->mutex);
  ++m_state->generation;
  m_state->pending.reset();
  m_state->scheduled = false;
  m_state->subscriptionEpoch = deskflow::input::inputIndicatorSubscriptionEpoch();
}

} // namespace deskflow::platform
