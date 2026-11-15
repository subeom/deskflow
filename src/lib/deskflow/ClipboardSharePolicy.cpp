/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardSharePolicy.h"

#include "deskflow/Clipboard.h"

#include <algorithm>
#include <string>

ClipboardSharePolicy::Decision ClipboardSharePolicy::decide(const Content &content) const
{
  if (!m_textOnly) {
    return {};
  }

  if (!content.text) {
    return {Action::Drop, Reason::NoText, 0};
  }

  // the text counts as it is marshalled: with the bytes marshall() adds around it, as every peer counts it
  const auto textBytes = *content.text;
  if (textBytes > kTextOnlyMaxTextBytes) {
    return {Action::Drop, Reason::TextTooLarge, textBytes};
  }

  return {Action::ShareText, Reason::None, textBytes};
}

ClipboardSharePolicy::Decision ClipboardSharePolicy::apply(Clipboard &clipboard) const
{
  if (!m_textOnly) {
    return {};
  }

  // opened at its own time, so that emptying it below does not change the time it was taken
  const auto time = clipboard.getTime();

  Content content;
  std::string text;
  if (clipboard.open(time)) {
    if (clipboard.has(IClipboard::Format::Text)) {
      text = clipboard.get(IClipboard::Format::Text);
      content.text = text.size();
    }
    content.html = clipboard.has(IClipboard::Format::HTML);
    content.bitmap = clipboard.has(IClipboard::Format::Bitmap);
    clipboard.close();
  }

  const auto decision = decide(content);
  if (decision.action == Action::ShareText && !content.html && !content.bitmap) {
    // nothing to take out
    return decision;
  }

  if (clipboard.open(time)) {
    clipboard.empty();
    if (decision.action == Action::ShareText) {
      clipboard.add(IClipboard::Format::Text, text);
    }
    clipboard.close();
  }

  return decision;
}

uint32_t ClipboardSharePolicy::sizeOptionKiB(uint32_t configuredMiB, bool textOnly)
{
  if (textOnly) {
    return kTextOnlyLimitBytes / 1024;
  }
  return configuredMiB * 1024;
}

bool ClipboardRejectionLog::report(const ClipboardSharePolicy::Decision &decision)
{
  if (decision.action != ClipboardSharePolicy::Action::Drop) {
    m_last = {};
    return false;
  }

  if (decision == m_last) {
    return false;
  }

  m_last = decision;
  return true;
}
