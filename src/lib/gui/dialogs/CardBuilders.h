/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QList>
#include <QString>
#include <QTextDocument>
#include <QWidget>

namespace deskflow::gui::cards {

//! The child of \p parent with this object name, as the type that is looked for
template <class T> T *find(QWidget *parent, const char *name)
{
  return parent->findChild<T *>(QString::fromLatin1(name));
}

//! A tooltip that is a line of text is guidance for the row; one in rich text (a list, italics) is not
inline QString plainTip(const QWidget *widget)
{
  const auto tip = widget->toolTip();
  return Qt::mightBeRichText(tip) ? QString() : tip;
}

//! "Name:" as the title of a row is "Name"
inline QString withoutColon(QString text)
{
  while (text.endsWith(QLatin1Char(':')) || text.endsWith(QLatin1Char(' ')))
    text.chop(1);
  return text;
}

//! "Experimental: multi-monitor aware cursor mapping" under a card that is called "Experimental" is "Multi-monitor
//! aware cursor mapping"
inline QString withoutPrefix(const QString &title, const QString &prefix)
{
  const auto lead = prefix + QStringLiteral(": ");
  if (prefix.isEmpty() || !title.startsWith(lead))
    return title;
  auto rest = title.mid(lead.size());
  if (!rest.isEmpty())
    rest[0] = rest[0].toUpper();
  return rest;
}

//! A row of a switch, with the controls that go before it (a number): the text of the switch is the title, and a
//! tooltip in plain text is the guidance under it
inline SettingRow *switchRow(ToggleSwitch *toggle, const QList<QWidget *> &before = {}, const QString &cardTitle = {})
{
  auto *row = new SettingRow;
  const auto tip = plainTip(toggle);
  if (!tip.isEmpty())
    toggle->setToolTip({});
  for (auto *control : before)
    row->addControl(control);
  row->addControl(toggle);
  if (!tip.isEmpty())
    row->setDescription(tip);
  if (!cardTitle.isEmpty())
    row->setTitle(withoutPrefix(row->rawTitle(), cardTitle));
  return row;
}

} // namespace deskflow::gui::cards
