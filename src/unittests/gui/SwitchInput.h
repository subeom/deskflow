/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/Appearance.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QImage>
#include <QTest>
#include <QWidget>

// What the tests of the switches need to try them as a person does: a click of the mouse and the key Space, and to look
// at what the switch paints afterwards. The seventh run of the checks of Windows found that the knob of every switch
// stood still when it was clicked, and the value was right: the tests had set the switches with setChecked(), which is
// the one way that Qt tells a button of a state that the program painted from a copy of its own
// (ToggleSwitch::checkStateSet); a click and the key Space go another way (nextCheckState). So these tests give the
// input and look at the picture.
namespace switchinput {

inline bool nearColor(const QColor &a, const QColor &b, int tolerance = 4)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

//! The name that says which switch it is: the names of the objects from the switch up to the widget that is searched
inline QString nameOf(const QWidget *toggle, const QWidget *root)
{
  QStringList names;
  for (const QWidget *w = toggle; w && w != root; w = w->parentWidget()) {
    if (!w->objectName().isEmpty())
      names.prepend(w->objectName());
  }
  return names.isEmpty() ? QStringLiteral("<unnamed switch>") : names.join(QLatin1Char('/'));
}

//! The middle of the knob in the picture of the switch when it is at \p position (0 at the off end, 1 at the on end):
//! the geometry of the paint of ToggleSwitch (3 px of room around the track for the ring, the knob 19 px, or 14 px
//! beside a text)
inline QPoint knobMiddle(const ToggleSwitch &toggle, qreal position)
{
  const bool withText = !toggle.text().isEmpty();
  const auto track = ToggleSwitch::trackSize(withText);
  const qreal diameter = withText ? 14.0 : 19.0;
  const qreal margin = (track.height() - diameter) / 2;
  const qreal travel = track.width() - 2 * margin - diameter;
  const qreal x = 3 + margin + position * travel + diameter / 2;
  const qreal y = toggle.height() / 2.0;
  return QPoint(qRound(x), qRound(y));
}

/*!
 * What is wrong with what the switch shows, as a sentence (empty when nothing is): the knob is at the end of the state
 * that the switch is in (it is let to arrive: a motion takes a moment), and in the picture the middle of the knob is
 * the colour of the knob of that state and the other end of the track is the accent where the switch is on.
 */
inline QString whatIsWrongWithTheKnob(ToggleSwitch &toggle, bool on)
{
  const qreal wanted = on ? 1.0 : 0.0;
  for (int i = 0; i < 200 && !qFuzzyCompare(toggle.knobPosition() + 1, wanted + 1); ++i)
    QTest::qWait(10);
  if (!qFuzzyCompare(toggle.knobPosition() + 1, wanted + 1))
    return QStringLiteral("the knob is at %1 and the switch is %2")
        .arg(toggle.knobPosition())
        .arg(on ? QStringLiteral("on") : QStringLiteral("off"));
  if (!deskflow::gui::style::isDeskflowStyleInstalled())
    return {};
  if (toggle.width() < 20 || toggle.height() < 10)
    return QStringLiteral("the switch has no size (%1 x %2)").arg(toggle.width()).arg(toggle.height());

  const auto &c = deskflow::gui::style::currentTokens().colors;
  const auto image = toggle.grab().toImage();
  const auto knob = knobMiddle(toggle, wanted);
  if (!image.rect().contains(knob))
    return QStringLiteral("the knob would be outside of the picture");
  const auto seen = image.pixelColor(knob);
  const auto expected = on ? c.onAccent : c.controlBorder;
  if (!nearColor(seen, expected))
    return QStringLiteral("the middle of the knob is %1 and the knob of a switch that is %2 is %3")
        .arg(seen.name(), on ? QStringLiteral("on") : QStringLiteral("off"), expected.name());
  if (on) {
    const auto track = image.pixelColor(knobMiddle(toggle, 0.0));
    if (!nearColor(track, c.accent))
      return QStringLiteral("the track of a switch that is on is %1 and not the accent %2")
          .arg(track.name(), c.accent.name());
  }
  return {};
}

struct Sweep
{
  int found = 0;
  int tried = 0;
  int disabled = 0;
  int notShown = 0;
  QStringList problems;
};

/*!
 * Gives every switch under \p root that can be used a click of the mouse and then the key Space (the switch is on or
 * off after the first, as it was after the second), and says what is wrong with the value and with what it paints. The
 * widgets have to be shown. The style of the new look has to be installed, as the people that have the switches have
 * it.
 */
inline Sweep sweepSwitches(QWidget *root)
{
  Sweep sweep;
  if (!deskflow::gui::style::isDeskflowStyleInstalled()) {
    sweep.problems << QStringLiteral("the style of the new look is not installed");
    return sweep;
  }
  for (auto *toggle : root->findChildren<ToggleSwitch *>()) {
    ++sweep.found;
    const auto name = nameOf(toggle, root);
    if (!toggle->isEnabled()) {
      ++sweep.disabled;
      continue;
    }
    if (!toggle->isVisibleTo(root)) {
      ++sweep.notShown;
      continue;
    }
    const bool before = toggle->isChecked();
    QTest::mouseClick(toggle, Qt::LeftButton);
    if (toggle->isChecked() == before) {
      sweep.problems << name + QStringLiteral(": a click did not change the value");
      continue;
    }
    if (const auto why = whatIsWrongWithTheKnob(*toggle, !before); !why.isEmpty())
      sweep.problems << name + QStringLiteral(": after a click of the mouse ") + why;
    QTest::keyClick(toggle, Qt::Key_Space);
    if (toggle->isChecked() != before) {
      sweep.problems << name + QStringLiteral(": the key Space did not change the value back");
      continue;
    }
    if (const auto why = whatIsWrongWithTheKnob(*toggle, before); !why.isEmpty())
      sweep.problems << name + QStringLiteral(": after the key Space ") + why;
    ++sweep.tried;
  }
  return sweep;
}

} // namespace switchinput
