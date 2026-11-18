/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "TextSizeModel.h"

#include <QFont>
#include <QObject>

#include <functional>

namespace deskflow::gui::style {

/*!
 * What the service asks the system. The defaults are the ones of this computer; the tests give their own.
 */
struct TextSizeEnvironment
{
  //! The font that the system gives text (not the font of the application, which the service changes)
  std::function<QFont()> systemFont;
  //! What the system has for the size of text apart from its font (Windows: Text size in the accessibility settings)
  std::function<double()> osTextScale;
  TextPlatform platform = TextPlatform::Other;

  //! The system font of this computer, Windows' own Text size where there is one, and this OS
  static TextSizeEnvironment ofThisComputer();
};

/*!
 * \brief The size of text of the new look: the body text is the size that the system gives (never smaller than the
 * least that the OS has, see minimumBodyPixels), as large as the person chose (the setting gui/textSize), and the sizes
 * of what carries text follow it (px()).
 *
 * It is made when the application has the new look, and never with the look of the system, which keeps the font of the
 * application as the OS gave it. It changes the font of the application (QApplication::setFont) when the size is not
 * the one of the system, which every widget is told of (QEvent::ApplicationFontChange). It follows the setting at once,
 * and reads the system again when the application becomes active (the person changed a setting of the OS).
 */
class TextSize : public QObject
{
  Q_OBJECT

public:
  //! The service, or null while the application has the look of the system
  static TextSize *instance();

  //! Makes the service and applies it. Call after the application object, before the first window.
  static TextSize *install(const TextSizeEnvironment &environment = TextSizeEnvironment::ofThisComputer());
  //! Puts the font of the system back, and the sizes as they were drawn; ends the service
  static void uninstall();

  TextSizePlan plan() const;
  int percent() const;
  //! The font that the system gives text (what the service started from)
  QFont systemFont() const;

  //! Sets the step: the setting is written, and the service follows it
  void setPercent(int percent);
  //! One step larger or smaller (direction > 0 or < 0); 0 is the default
  void step(int direction);

  //! Asks the system again for its font and its size of text
  void refresh();

Q_SIGNALS:
  //! The size of text is another (the font of the application and the scale of the controls)
  void planChanged(const deskflow::gui::style::TextSizePlan &plan);

private:
  explicit TextSize(const TextSizeEnvironment &environment);
  ~TextSize() override;

  void apply();
  void followSetting(const QString &key);
  double systemPixels(const QFont &font) const;
  double pixelsPerPoint(const QFont &font) const;

  TextSizeEnvironment m_environment;
  TextSizePlan m_plan;
  QFont m_system;
  int m_percent = kDefaultTextPercent;
  bool m_fontIsOurs = false;
};

/*!
 * Tells that the size of text is another. It is there whether the service is or not (it is made when the application
 * starts and is told by the service), so that a widget connects once. Qt tells the windows of a new font of the
 * application, and a widget whose font was set by the program (a title, a hint) is not told of it; this is for those.
 */
class TextSizeNotifier : public QObject
{
  Q_OBJECT

public:
  static TextSizeNotifier *instance();

Q_SIGNALS:
  void changed();

private:
  using QObject::QObject;
  friend class TextSize;
};

//! Calls the function when the size of text changes: for a widget that sets fonts and sizes of its own. The connection
//! ends with the context object.
QMetaObject::Connection onTextSizeChanged(const QObject *context, const std::function<void()> &callback);

//! The scale of the controls (1 while the application has the look of the system)
double metricScale();

//! A size in logical pixels of what carries text or is touched, made larger as the text is: a row, a switch, a chip.
//! Sizes of decoration (a radius, a gap, a border) are not.
int px(int logicalPixels);

} // namespace deskflow::gui::style
