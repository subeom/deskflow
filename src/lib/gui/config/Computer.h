/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ComputerConfig.h"

#include "common/KeyboardModifier.h"
#include "common/QSettingsProxy.h"
#include "deskflow/OptionTypes.h"

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

class QSettings;
class QTextStream;
class ComputerSettingsDialog;

class Computer : public ComputerConfig
{
  friend class ComputerSettingsDialog;
  friend class ComputerSetupModel;
  friend class ComputerSetupView;

  friend QDataStream &operator<<(QDataStream &outStream, const Computer &computer)
  {
    return outStream << computer.name() << computer.switchCornerSize() << computer.aliases() << computer.modifiers()
                     << computer.switchCorners() << computer.fixes() << computer.isServer()
                     << static_cast<quint8>(computer.m_F18Action) << computer.m_CrossOSInvertX
                     << computer.m_CrossOSInvertY << computer.m_CrossOSSemanticMedia
                     << static_cast<quint8>(computer.m_CrossOSInvertScope) << computer.m_ShareClipboard
                     << computer.m_MonitorLayout;
  }

  friend QDataStream &operator>>(QDataStream &inStream, Computer &computer)
  {
    quint8 f18Action = 0;
    quint8 crossOSInvertScope = 0;
    inStream >> computer.m_Name >> computer.m_SwitchCornerSize >> computer.m_Aliases >> computer.m_Modifiers >>
        computer.m_SwitchCorners >> computer.m_Fixes >> computer.m_isServer >> f18Action >> computer.m_CrossOSInvertX >>
        computer.m_CrossOSInvertY >> computer.m_CrossOSSemanticMedia >> crossOSInvertScope >>
        computer.m_ShareClipboard >> computer.m_MonitorLayout;
    computer.m_F18Action =
        f18Action == static_cast<quint8>(F18Action::Hangul) ? F18Action::Hangul : F18Action::PassThrough;
    computer.m_CrossOSInvertScope = crossOSInvertScope == static_cast<quint8>(ScrollInvertScope::MouseWheelOnly)
                                        ? ScrollInvertScope::MouseWheelOnly
                                        : ScrollInvertScope::All;
    return inStream;
  }

public:
  explicit Computer(const QString &name = QString());

  [[nodiscard]] const QPixmap &pixmap() const
  {
    return m_Pixmap;
  }
  [[nodiscard]] const QString &name() const
  {
    return m_Name;
  }
  [[nodiscard]] const QStringList &aliases() const
  {
    return m_Aliases;
  }

  [[nodiscard]] bool isNull() const
  {
    return m_Name.isEmpty();
  }
  [[nodiscard]] int modifier(int m) const
  {
    return m_Modifiers[m] == static_cast<int>(KeyboardModifier::DefaultMod) ? m : m_Modifiers[m];
  }
  [[nodiscard]] const QList<int> &modifiers() const
  {
    return m_Modifiers;
  }
  [[nodiscard]] bool switchCorner(int c) const
  {
    return m_SwitchCorners[c];
  }
  [[nodiscard]] const QList<bool> &switchCorners() const
  {
    return m_SwitchCorners;
  }
  [[nodiscard]] int switchCornerSize() const
  {
    return m_SwitchCornerSize;
  }
  [[nodiscard]] bool fix(const Fix f) const
  {
    return m_Fixes[static_cast<int8_t>(f)];
  }
  [[nodiscard]] const QList<bool> &fixes() const
  {
    return m_Fixes;
  }
  [[nodiscard]] F18Action f18Action() const
  {
    return m_F18Action;
  }
  [[nodiscard]] bool crossOSInvertX() const
  {
    return m_CrossOSInvertX;
  }
  [[nodiscard]] bool crossOSInvertY() const
  {
    return m_CrossOSInvertY;
  }
  [[nodiscard]] ScrollInvertScope crossOSInvertScope() const
  {
    return m_CrossOSInvertScope;
  }
  [[nodiscard]] bool crossOSSemanticMedia() const
  {
    return m_CrossOSSemanticMedia;
  }
  [[nodiscard]] bool shareClipboard() const
  {
    return m_ShareClipboard;
  }
  //! The monitors of the computer (experimental), as the text that was set: empty when there is none
  [[nodiscard]] const QString &monitorLayout() const
  {
    return m_MonitorLayout;
  }

  void loadSettings(QSettingsProxy &settings);
  void saveSettings(QSettingsProxy &settings) const;
  [[nodiscard]] QString screensSection() const;

  [[nodiscard]] bool swapped() const
  {
    return m_Swapped;
  }

  void setName(const QString &name)
  {
    m_Name = name;
  }
  [[nodiscard]] bool isServer() const
  {
    return m_isServer;
  }
  void markAsServer()
  {
    m_isServer = true;
  }

  bool operator==(const Computer &computer) const;

protected:
  QStringList &aliases()
  {
    return m_Aliases;
  }
  void setModifier(const KeyboardModifier m, const int n)
  {
    m_Modifiers[static_cast<int8_t>(m)] = n;
  }
  QList<int> &modifiers()
  {
    return m_Modifiers;
  }
  void addAlias(const QString &alias)
  {
    m_Aliases.append(alias);
  }
  void setSwitchCorner(const SwitchCorner c, const bool on)
  {
    m_SwitchCorners[static_cast<int8_t>(c)] = on;
  }
  QList<bool> &switchCorners()
  {
    return m_SwitchCorners;
  }
  void setSwitchCornerSize(const int val)
  {
    m_SwitchCornerSize = val;
  }
  void setFix(const Fix f, const bool on)
  {
    m_Fixes[static_cast<int8_t>(f)] = on;
  }
  QList<bool> &fixes()
  {
    return m_Fixes;
  }
  void setF18Action(const F18Action action)
  {
    m_F18Action = action;
  }
  void setCrossOSInvertX(const bool enabled)
  {
    m_CrossOSInvertX = enabled;
  }
  void setCrossOSInvertY(const bool enabled)
  {
    m_CrossOSInvertY = enabled;
  }
  void setCrossOSInvertScope(const ScrollInvertScope scope)
  {
    m_CrossOSInvertScope = scope;
  }
  void setCrossOSSemanticMedia(const bool enabled)
  {
    m_CrossOSSemanticMedia = enabled;
  }
  void setShareClipboard(const bool enabled)
  {
    m_ShareClipboard = enabled;
  }
  void setMonitorLayout(const QString &layout)
  {
    m_MonitorLayout = layout.trimmed();
  }
  void setSwapped(const bool on)
  {
    m_Swapped = on;
  }

private:
  QPixmap m_Pixmap = QIcon::fromTheme("video-display").pixmap(QSize(96, 96));
  QString m_Name = {};
  QStringList m_Aliases = {};
  QList<int> m_Modifiers = {0, 1, 2, 3, 4, 5, 6};
  QList<bool> m_SwitchCorners = {false, false, false, false};
  int m_SwitchCornerSize = 0;
  QList<bool> m_Fixes{false, false, false, false};
  F18Action m_F18Action = F18Action::PassThrough;
  bool m_CrossOSInvertX = false;
  bool m_CrossOSInvertY = false;
  ScrollInvertScope m_CrossOSInvertScope = ScrollInvertScope::All;
  bool m_CrossOSSemanticMedia = false;
  bool m_ShareClipboard = true;
  QString m_MonitorLayout = {};
  bool m_Swapped = false;
  bool m_isServer = false;
};
