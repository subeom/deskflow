/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IClipboard.h"

#include <mutex>

//! Memory buffer clipboard
/*!
This class implements a clipboard that stores data in memory.
*/
class Clipboard : public IClipboard
{
public:
  Clipboard();
  ~Clipboard() override = default;

  //! @name manipulators
  //@{

  //! Unmarshall clipboard data
  /*!
  Extract marshalled clipboard data and store it in this clipboard.
  Sets the clipboard time to \c time.
  */
  void unmarshall(const std::string &data, Time time);

  //@}
  //! @name accessors
  //@{

  //! Marshall clipboard data
  /*!
  Merge this clipboard's data into a single buffer that can be later
  unmarshalled to restore the clipboard and return the buffer.
  */
  std::string marshall() const;

  //@}

  //! Hold text only
  /*!
  A clipboard that holds text only wants no other format: add() ignores them, and copy() into this clipboard does not
  read them from its source.  The data it holds now is not touched.
  */
  void setTextOnly(bool textOnly);

  //! Set the size limit of the clipboard that is shared, in bytes (0: none), see IClipboard::sizeLimit()
  void setSizeLimit(size_t bytes);

  // IClipboard overrides
  bool empty() final;
  void add(Format, const std::string &data) override;
  bool open(Time) const final;
  void close() const override;
  Time getTime() const override;
  bool has(Format) const override;
  bool wants(Format) const override;
  size_t sizeLimit() const override;
  std::string get(Format) const override;

private:
  mutable bool m_open = false;
  mutable std::mutex m_mutex;
  mutable Time m_time;
  bool m_owner = false;
  bool m_textOnly = false;
  size_t m_sizeLimit = 0;
  Time m_timeOwned;
  bool m_added[static_cast<int>(Format::TotalFormats)] = {false, false, false};
  std::string m_data[static_cast<int>(Format::TotalFormats)] = {"", "", ""};
};
