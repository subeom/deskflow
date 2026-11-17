/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/style/DesignTokens.h"

#include <QWidget>

/*!
 * \brief A small capsule with a state in it: an icon and a word on a soft fill of the colour of the state. The state is
 * told by the icon and the word as well as by the colour, so that it reads without telling colours apart, and a screen
 * reader says "Warning: Experimental" for it.
 */
class StatusChip : public QWidget
{
  Q_OBJECT

public:
  enum class Kind
  {
    Neutral,
    Info,
    Success,
    Warning,
    Danger
  };
  Q_ENUM(Kind)

  explicit StatusChip(QWidget *parent = nullptr);
  explicit StatusChip(const QString &text, Kind kind = Kind::Neutral, QWidget *parent = nullptr);

  QString text() const;
  void setText(const QString &text);

  Kind kind() const;
  void setKind(Kind kind);

  //! The icon of a kind (a name of the Phosphor set); empty for Neutral
  static QString iconName(Kind kind);
  //! The word that tells the kind besides its colour ("Warning"); empty for Neutral
  static QString kindWord(Kind kind);

  //! The fill and the text of a kind, with the contrast that the text needs on the fill
  struct Palette
  {
    QColor fill;
    QColor text;
  };
  static Palette paletteFor(Kind kind, const deskflow::gui::style::Tokens &tokens);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void updateAccessibleName();

  QString m_text;
  Kind m_kind = Kind::Neutral;
};
