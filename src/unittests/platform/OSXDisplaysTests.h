/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class OSXDisplaysTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void rectanglesAreThoseOfTheRectangleList();
  void boxOfTheMonitorsIsTheShapeOfTheComputer();
  void scaleAndPrimaryAreReported();
  void monitorsCanBeReported();
  void detailsAreOfTheSameDisplaysAsTheInfo();
};
