/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/LayoutProfileStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace deskflow::gui::layout {

namespace {

const auto kFormatName = QStringLiteral("deskflow-layout-profiles");
// a time in milliseconds that is a time: within the range of a double that counts whole numbers, and not negative
constexpr double kMaxTime = 1.0e15;
constexpr int kMaxIdLength = 64;
constexpr int kNamedDisplays = 4;

bool validId(const QString &id)
{
  if (id.isEmpty() || id.size() > kMaxIdLength)
    return false;
  return std::ranges::all_of(id, [](QChar c) { return c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'); });
}

bool wholeNumber(const QJsonValue &value, double min, double max, int64_t *out)
{
  if (!value.isDouble())
    return false;
  const double number = value.toDouble();
  if (!std::isfinite(number) || number != std::floor(number) || number < min || number > max)
    return false;
  *out = static_cast<int64_t>(number);
  return true;
}

// the number in an id like p17, or 0
int64_t numberOfId(const QString &id)
{
  if (!id.startsWith(QLatin1Char('p')))
    return 0;
  bool ok = false;
  const auto number = id.mid(1).toLongLong(&ok);
  // a number that is not one that this program made is no number: the next one is made from it by adding one
  return ok && number > 0 && number <= static_cast<int64_t>(kMaxTime) ? number : 0;
}

// whether \p a is removed before \p b when there are too many: what the user did not name first, then the one that was
// not used for the longest time, then the one that was made first, then the smaller id
bool goesBefore(const Profile &a, const Profile &b)
{
  if (a.userNamed != b.userNamed)
    return !a.userNamed;
  if (a.lastUsedAt != b.lastUsedAt)
    return a.lastUsedAt < b.lastUsedAt;
  if (a.createdAt != b.createdAt)
    return a.createdAt < b.createdAt;
  return a.id < b.id;
}

QString cappedName(const QString &name)
{
  return name.trimmed().left(LayoutProfileStore::kMaxNameLength).trimmed();
}

std::optional<Profile> profileFromJson(const QJsonObject &object)
{
  Profile profile;
  const auto id = object.value(QStringLiteral("id"));
  if (!id.isString() || !validId(id.toString()))
    return std::nullopt;
  profile.id = id.toString();

  const auto signature = object.value(QStringLiteral("signature"));
  const auto layout = object.value(QStringLiteral("layout"));
  if (!signature.isObject() || !layout.isObject())
    return std::nullopt;
  const auto parsedSignature = DisplaySignature::fromJson(signature.toObject());
  const auto parsedLayout = Layout::fromJson(layout.toObject());
  if (!parsedSignature || !parsedLayout)
    return std::nullopt;
  profile.signature = *parsedSignature;
  profile.layout = *parsedLayout;

  const auto name = object.value(QStringLiteral("name"));
  if (!name.isUndefined() && !name.isString())
    return std::nullopt;
  profile.name = cappedName(name.toString());
  if (profile.name.isEmpty())
    profile.name = LayoutProfileStore::suggestName(profile.signature);

  const auto named = object.value(QStringLiteral("userNamed"));
  if (!named.isUndefined() && !named.isBool())
    return std::nullopt;
  profile.userNamed = named.toBool(false);

  if (!wholeNumber(object.value(QStringLiteral("createdAt")), 0, kMaxTime, &profile.createdAt) ||
      !wholeNumber(object.value(QStringLiteral("updatedAt")), 0, kMaxTime, &profile.updatedAt) ||
      !wholeNumber(object.value(QStringLiteral("lastUsedAt")), 0, kMaxTime, &profile.lastUsedAt))
    return std::nullopt;
  return profile;
}

QJsonObject toJson(const Profile &profile)
{
  return QJsonObject{
      {QStringLiteral("id"), profile.id},
      {QStringLiteral("name"), profile.name},
      {QStringLiteral("userNamed"), profile.userNamed},
      {QStringLiteral("signature"), profile.signature.toJson()},
      {QStringLiteral("layout"), profile.layout.toJson()},
      {QStringLiteral("createdAt"), static_cast<double>(profile.createdAt)},
      {QStringLiteral("updatedAt"), static_cast<double>(profile.updatedAt)},
      {QStringLiteral("lastUsedAt"), static_cast<double>(profile.lastUsedAt)},
  };
}

} // namespace

LayoutProfileStore::LayoutProfileStore(const QString &path) : m_path(path)
{
}

LayoutProfileStore::LoadReport LayoutProfileStore::load()
{
  m_profiles.clear();
  m_active.reset();
  m_inert = false;
  m_lastError.clear();
  m_nextId = 1;

  LoadReport report;
  if (!QFileInfo::exists(m_path)) {
    report.status = Status::Missing;
    return report;
  }

  // a file that cannot be used is put aside, so that it is not read again and not written over
  const auto damaged = [&](const QString &why) {
    report.status = Status::Damaged;
    const auto aside = m_path + QStringLiteral(".damaged");
    QFile::remove(aside);
    if (QFile::rename(m_path, aside))
      report.keptAs = aside;
    report.message = QStringLiteral("the file of remembered layouts is not usable (%1); it was kept as %2")
                         .arg(why, report.keptAs.isEmpty() ? QStringLiteral("(it could not be moved)") : aside);
    m_lastError = report.message;
    return report;
  };

  QFile file(m_path);
  if (!file.open(QIODevice::ReadOnly)) {
    report.status = Status::Unreadable;
    report.message = QStringLiteral("the file of remembered layouts cannot be read: %1").arg(file.errorString());
    m_lastError = report.message;
    return report;
  }
  if (file.size() > kMaxFileBytes)
    return damaged(QStringLiteral("it is bigger than %1 bytes").arg(kMaxFileBytes));
  const auto bytes = file.readAll();
  file.close();

  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
    return damaged(QStringLiteral("it is not JSON"));
  const auto root = document.object();
  if (root.value(QStringLiteral("format")).toString() != kFormatName)
    return damaged(QStringLiteral("it is not a file of remembered layouts"));
  int64_t version = 0;
  if (!wholeNumber(root.value(QStringLiteral("version")), 1, 1.0e9, &version))
    return damaged(QStringLiteral("it has no version"));
  if (version > kFormatVersion) {
    report.status = Status::NewerVersion;
    m_inert = true;
    report.message = QStringLiteral("the file of remembered layouts is of a newer version (%1): it is not used and "
                                    "not changed")
                         .arg(version);
    m_lastError = report.message;
    return report;
  }
  if (!root.value(QStringLiteral("profiles")).isArray())
    return damaged(QStringLiteral("it has no list of profiles"));

  int64_t highestId = 0;
  for (const auto &entry : root.value(QStringLiteral("profiles")).toArray()) {
    auto profile = entry.isObject() ? profileFromJson(entry.toObject()) : std::nullopt;
    if (!profile || find(profile->id) != nullptr) {
      ++report.dropped;
      continue;
    }
    // two profiles of the same displays: the one that was used last is the one
    const auto same = std::ranges::find_if(m_profiles, [&](const Profile &p) {
      return p.signature.key() == profile->signature.key();
    });
    highestId = std::max(highestId, numberOfId(profile->id));
    if (same == m_profiles.end()) {
      m_profiles.push_back(*profile);
    } else {
      ++report.dropped;
      if (profile->lastUsedAt > same->lastUsedAt)
        *same = *profile;
    }
  }

  if (const auto active = root.value(QStringLiteral("active")); active.isObject())
    m_active = DisplaySignature::fromJson(active.toObject());
  int64_t nextId = 0;
  if (wholeNumber(root.value(QStringLiteral("nextId")), 1, kMaxTime, &nextId))
    m_nextId = nextId;
  m_nextId = std::max(m_nextId, highestId + 1);

  const auto before = m_profiles.size();
  evictToFit(QString());
  report.dropped += static_cast<int>(before - m_profiles.size());

  report.status = Status::Ok;
  if (report.dropped > 0) {
    // what was not understood is kept, once, before the file is written again
    const auto copy = m_path + QStringLiteral(".bak");
    QFile::remove(copy);
    if (QFile::copy(m_path, copy))
      report.keptAs = copy;
    report.message = QStringLiteral("%1 remembered layouts of the file were not valid or too many and were left out")
                         .arg(report.dropped);
  }
  return report;
}

bool LayoutProfileStore::save()
{
  if (m_inert) {
    m_lastError = QStringLiteral("the file of remembered layouts is of a newer version and is not changed");
    return false;
  }

  QJsonArray profiles;
  for (const auto &profile : m_profiles)
    profiles.append(toJson(profile));
  QJsonObject root{
      {QStringLiteral("format"), kFormatName},
      {QStringLiteral("version"), kFormatVersion},
      {QStringLiteral("nextId"), static_cast<double>(m_nextId)},
      {QStringLiteral("profiles"), profiles},
  };
  if (m_active)
    root.insert(QStringLiteral("active"), m_active->toJson());

  const auto folder = QFileInfo(m_path).absolutePath();
  if (!QFileInfo::exists(folder))
    QDir().mkpath(folder);

  // the file is replaced as a whole or not at all
  QSaveFile out(m_path);
  if (!out.open(QIODevice::WriteOnly)) {
    m_lastError = out.errorString();
    return false;
  }
  out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  if (!out.commit()) {
    m_lastError = out.errorString();
    return false;
  }
  m_lastError.clear();
  return true;
}

const Profile *LayoutProfileStore::find(const QString &id) const
{
  const auto found = std::ranges::find_if(m_profiles, [&](const Profile &p) { return p.id == id; });
  return found == m_profiles.end() ? nullptr : &*found;
}

const Profile *LayoutProfileStore::findExact(const DisplaySignature &signature) const
{
  if (!signature.isValid())
    return nullptr;
  const auto key = signature.key();
  const auto found = std::ranges::find_if(m_profiles, [&](const Profile &p) { return p.signature.key() == key; });
  return found == m_profiles.end() ? nullptr : &*found;
}

std::vector<Candidate> LayoutProfileStore::candidates() const
{
  std::vector<Candidate> list;
  for (const auto &profile : m_profiles)
    list.push_back({&profile.signature, profile.lastUsedAt, profile.createdAt, profile.id});
  return list;
}

LayoutProfileStore::Upsert LayoutProfileStore::upsert(
    const DisplaySignature &signature, const Layout &layout, int64_t now, const QString &name, bool userNamed
)
{
  Upsert result;
  if (!signature.isValid() || layout.isEmpty() || layout.computers.size() > static_cast<size_t>(Layout::kMaxComputers))
    return result;

  const auto key = signature.key();
  const auto existing = std::ranges::find_if(m_profiles, [&](const Profile &p) { return p.signature.key() == key; });
  if (existing != m_profiles.end()) {
    if (!layout.sameAs(existing->layout)) {
      existing->layout = layout;
      existing->updatedAt = now;
      result.changed = true;
    }
    existing->lastUsedAt = now;
    result.id = existing->id;
    return result;
  }

  Profile profile;
  profile.id = newId();
  profile.name = cappedName(name);
  profile.userNamed = userNamed && !profile.name.isEmpty();
  if (profile.name.isEmpty())
    profile.name = suggestName(signature);
  profile.signature = signature;
  profile.layout = layout;
  profile.createdAt = profile.updatedAt = profile.lastUsedAt = now;
  result.id = profile.id;
  result.created = true;
  m_profiles.push_back(profile);
  evictToFit(profile.id);
  return result;
}

void LayoutProfileStore::touch(const QString &id, int64_t now)
{
  const auto found = std::ranges::find_if(m_profiles, [&](const Profile &p) { return p.id == id; });
  if (found != m_profiles.end())
    found->lastUsedAt = now;
}

bool LayoutProfileStore::rename(const QString &id, const QString &name, int64_t now)
{
  const auto capped = cappedName(name);
  if (capped.isEmpty())
    return false;
  const auto found = std::ranges::find_if(m_profiles, [&](const Profile &p) { return p.id == id; });
  if (found == m_profiles.end())
    return false;
  found->name = capped;
  found->userNamed = true;
  found->updatedAt = now;
  return true;
}

bool LayoutProfileStore::remove(const QString &id)
{
  const auto found = std::ranges::find_if(m_profiles, [&](const Profile &p) { return p.id == id; });
  if (found == m_profiles.end())
    return false;
  m_profiles.erase(found);
  return true;
}

QString LayoutProfileStore::suggestName(const DisplaySignature &signature)
{
  if (!signature.isValid())
    return QStringLiteral("no displays");
  QStringList parts;
  const auto &displays = signature.displays();
  for (size_t i = 0; i < displays.size() && i < static_cast<size_t>(kNamedDisplays); ++i) {
    parts << QStringLiteral("%1 %2x%3")
                 .arg(
                     displays[i].builtIn ? QStringLiteral("built-in") : QStringLiteral("external"),
                     QString::number(displays[i].w), QString::number(displays[i].h)
                 );
  }
  auto name = parts.join(QStringLiteral(" + "));
  if (displays.size() > static_cast<size_t>(kNamedDisplays))
    name += QStringLiteral(" + %1 more").arg(displays.size() - static_cast<size_t>(kNamedDisplays));
  return name.left(kMaxNameLength);
}

QString LayoutProfileStore::newId()
{
  QString id;
  do {
    id = QStringLiteral("p%1").arg(m_nextId++);
  } while (find(id) != nullptr);
  return id;
}

void LayoutProfileStore::evictToFit(const QString &keepId)
{
  const auto activeKey = m_active ? m_active->key() : QString();
  while (m_profiles.size() > static_cast<size_t>(kMaxProfiles)) {
    // the profile that was just made or used, and the one that the arrangement belongs to, stay
    const Profile *victim = nullptr;
    for (const auto &profile : m_profiles) {
      if (profile.id == keepId || (!activeKey.isEmpty() && profile.signature.key() == activeKey))
        continue;
      if (victim == nullptr || goesBefore(profile, *victim))
        victim = &profile;
    }
    if (victim == nullptr)
      break;
    const auto victimId = victim->id;
    remove(victimId);
  }
}

} // namespace deskflow::gui::layout
