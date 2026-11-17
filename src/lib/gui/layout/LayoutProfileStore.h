/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/DisplaySignature.h"
#include "gui/layout/Layout.h"
#include "gui/layout/LayoutMatcher.h"

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace deskflow::gui::layout {

//! One remembered situation: the displays of the server, and how the other computers were arranged then
struct Profile
{
  //! Never shown; what the profile is called in the file and in the log
  QString id;
  QString name;
  //! The name was chosen by the user: such a profile is the last to go when there are too many
  bool userNamed = false;
  DisplaySignature signature;
  Layout layout;
  //! Milliseconds since the epoch
  int64_t createdAt = 0;
  int64_t updatedAt = 0;
  int64_t lastUsedAt = 0;
};

//! The remembered situations, and the file that keeps them (experimental: layout profiles)
/*!
A file of its own, not the settings: the settings file is rewritten as a whole by two programs, is read by the core, and a
damaged value in it could keep Deskflow from starting; this file is read and written by the GUI only, is replaced
atomically, and a damaged one is put aside and never read by anything else.

The file is JSON: \c format, \c version (1), \c nextId, \c active (the situation that the arrangement in the settings
belongs to), and \c profiles. A file of a newer version is not read and not written (the store is \e inert, the file is
left as it is); a file that is damaged is moved to \c <file>.damaged and the store starts empty; an entry that is not
valid is left out, and the file as it was is kept as \c <file>.bak. At most \c kMaxProfiles are kept: when there are more,
the least recently used one goes, one that the user did not name before one that the user did; the situation that is
active, and the profile that was just made or used, are not removed.
*/
class LayoutProfileStore
{
public:
  static constexpr int kFormatVersion = 1;
  static constexpr int kMaxProfiles = 32;
  static constexpr int kMaxNameLength = 128;
  //! A file bigger than this is not read
  static constexpr qint64 kMaxFileBytes = 2 * 1024 * 1024;

  enum class Status
  {
    //! The file was read
    Ok,
    //! There is no file: the store is empty
    Missing,
    //! The file could not be read as a store: it was moved aside, the store is empty
    Damaged,
    //! The file is of a newer version: the store is empty and inert
    NewerVersion,
    //! The file could not be opened
    Unreadable
  };

  struct LoadReport
  {
    Status status = Status::Missing;
    //! Entries of a file that was read, that were not valid or were too many, and are gone
    int dropped = 0;
    //! Where a damaged file was moved, or the copy of a file that lost entries
    QString keptAs;
    QString message;
  };

  explicit LayoutProfileStore(const QString &path);

  const QString &path() const
  {
    return m_path;
  }

  //! Reads the file, if there is one; whatever was in the store is gone
  LoadReport load();
  //! Writes the file as a whole, atomically; nothing is written for an inert store. False if it could not be written
  bool save();
  bool isInert() const
  {
    return m_inert;
  }
  const QString &lastError() const
  {
    return m_lastError;
  }

  const std::vector<Profile> &profiles() const
  {
    return m_profiles;
  }
  const Profile *find(const QString &id) const;
  //! The profile of exactly these displays
  const Profile *findExact(const DisplaySignature &signature) const;
  //! For the matcher; the pointers are good until the store changes
  std::vector<Candidate> candidates() const;

  struct Upsert
  {
    //! The profile now: nothing when it was not valid
    QString id;
    bool created = false;
    //! The layout of an existing profile was another one
    bool changed = false;
  };
  //! The profile of exactly these displays gets \p layout (when it is another), or is made
  /*!
  A made profile is called \p name when that is not empty, else what \c suggestName says, and is user named when
  \p userNamed. The profile is marked as used at \p now. Nothing is stored for a signature or a layout that is not valid.
  */
  Upsert upsert(
      const DisplaySignature &signature, const Layout &layout, int64_t now, const QString &name = QString(),
      bool userNamed = false
  );
  //! A profile was used at \p now
  void touch(const QString &id, int64_t now);
  //! Gives a profile a name of the user's; false for an empty name or an unknown profile
  bool rename(const QString &id, const QString &name, int64_t now);
  bool remove(const QString &id);

  //! The situation that the arrangement of the settings belongs to
  std::optional<DisplaySignature> active() const
  {
    return m_active;
  }
  void setActive(const std::optional<DisplaySignature> &signature)
  {
    m_active = signature;
  }

  //! What a profile of these displays is called when the user has not said: \c "external 3440x1440 + built-in 1512x982"
  static QString suggestName(const DisplaySignature &signature);

private:
  QString newId();
  void evictToFit(const QString &keepId);

  QString m_path;
  std::vector<Profile> m_profiles;
  std::optional<DisplaySignature> m_active;
  int64_t m_nextId = 1;
  bool m_inert = false;
  QString m_lastError;
};

} // namespace deskflow::gui::layout
