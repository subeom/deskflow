/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

class Clipboard;

//! The "share text only, up to 10 KB" clipboard mode
/*!
By default a clipboard is shared as a whole, with every format it holds, and only the size limit the user
configured applies.  In text-only mode only plain text is shared, and only while the marshalled clipboard (see
IClipboard::marshall()) fits in \c kTextOnlyLimitBytes.  Text that is too long is not shared at all, never cut short,
because a truncated text is a corrupted text.  Images, HTML and every other format are never shared.

The limit is that of the marshalled clipboard because that is the number every peer compares with the server's
clipboard size option (CLSZ, in KiB): the server when it sends and receives, a client when it sends and receives.  A
clipboard that holds only text marshals to \c kMarshalledTextOverheadBytes more than the text itself.

This class decides; it does not touch the network or the operating system, so it can be tested on its own.
*/
class ClipboardSharePolicy
{
public:
  //! Largest marshalled clipboard that is shared in text-only mode, in bytes (10 KiB)
  static constexpr size_t kTextOnlyLimitBytes = 10 * 1024;

  //! What IClipboard::marshall() adds to a clipboard that holds only text
  /*!
  4 bytes for the number of formats, 4 for the format and 4 for the size of the text.
  */
  static constexpr size_t kMarshalledTextOverheadBytes = 12;

  //! Longest text that is shared in text-only mode, in bytes of UTF-8
  static constexpr size_t kTextOnlyMaxTextBytes = kTextOnlyLimitBytes - kMarshalledTextOverheadBytes;

  //! What a clipboard holds, as far as sharing it is concerned
  struct Content
  {
    //! Size of the text in bytes, empty if the clipboard holds no text
    std::optional<size_t> text;
    bool html = false;
    bool bitmap = false;
  };

  enum class Action
  {
    ShareAll,  //!< share the clipboard as it is (not in text-only mode)
    ShareText, //!< share the text only
    Drop       //!< share nothing
  };

  enum class Reason
  {
    None,
    NoText,      //!< the clipboard holds no text (an image, HTML only, or nothing)
    TextTooLarge //!< the text does not fit in the limit
  };

  struct Decision
  {
    Action action = Action::ShareAll;
    Reason reason = Reason::None;
    //! Size of the text in bytes, 0 if there is none
    size_t textBytes = 0;

    bool operator==(const Decision &) const = default;
  };

  explicit ClipboardSharePolicy(bool textOnly = false) : m_textOnly(textOnly)
  {
  }

  bool textOnly() const
  {
    return m_textOnly;
  }

  //! What to do with a clipboard that holds \p content
  Decision decide(const Content &content) const;

  //! Do what decide() says about \p clipboard, in place
  /*!
  Leaves a clipboard that is shared as a whole alone, removes every format but the text, or empties the clipboard
  if nothing of it is shared.  Returns the decision.
  */
  Decision apply(Clipboard &clipboard) const;

  //! The clipboard size option (CLSZ, in KiB) to send for a configured size of \p configuredMiB MiB
  /*!
  Not in text-only mode this is the configured size, as it always was (0 turns sharing off).  In text-only mode it is
  the text-only limit whatever the configured size is: the size box is replaced by that fixed limit, so a 0 left in
  it must not keep sharing off behind a setting that promises text.
  */
  static uint32_t sizeOptionKiB(uint32_t configuredMiB, bool textOnly);

private:
  bool m_textOnly = false;
};

//! Remembers the clipboard that was not shared last, so that it is reported once and not on every event
class ClipboardRejectionLog
{
public:
  //! Whether \p decision is worth a report
  /*!
  True for a clipboard that is not shared and differs from the one that was reported last (in why it is not shared or
  in the size of its text); false for the same one again.  A clipboard that is shared re-arms the report.
  */
  bool report(const ClipboardSharePolicy::Decision &decision);

private:
  ClipboardSharePolicy::Decision m_last;
};
