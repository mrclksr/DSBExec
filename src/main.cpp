/*-
 * Copyright (c) 2024 Marcel Kaiser. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include "defs.h"
#include "mainwin.h"
#include "qt-helper/qt-helper.h"

int main(int argc, char *argv[]) {
  if (dsbexec_running()) return (EXIT_SUCCESS);
  QApplication app(argc, argv);
  QTranslator translator;

  if (translator.load(QLocale(), QLatin1String(PROGRAM), QLatin1String("_"),
                      QLatin1String(PATH_LOCALE)))
    app.installTranslator(&translator);
  MainWin w;

  return (app.exec());
}
