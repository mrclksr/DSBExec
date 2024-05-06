/*-
 * Copyright (c) 2024 Marcel Kaiser. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include "mainwin.h"

#include <QApplication>
#include <QDir>
#include <QScreen>
#include <QStringConverter>

#include "qt-helper/qt-helper.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent) {
  edit = new QLineEdit(this);
  QString prompt = QString(tr("Command:"));
  QIcon pic = qh::loadIcon(QStringList("system-run"));
  rootCb = new QCheckBox(tr("Execute as &root"));
  statusMsg = new QLabel(this);
  statusBar = new QStatusBar(this);
  QLabel *icon = new QLabel(this);
  QLabel *label = new QLabel(prompt);
  QVBoxLayout *vbox = new QVBoxLayout;
  QVBoxLayout *evbox = new QVBoxLayout;
  QHBoxLayout *hbox = new QHBoxLayout;
  QWidget *container = new QWidget(this);

  icon->setPixmap(pic.pixmap(64));
  setWindowIcon(pic);
  label->setStyleSheet("font-weight: bold;");

  hbox->addWidget(icon, 0, Qt::AlignLeft);
  vbox->addLayout(hbox);

  evbox->addWidget(label, 1, Qt::AlignLeft);
  evbox->addWidget(edit);
  evbox->addWidget(statusMsg);
  hbox->addLayout(evbox);

  vbox->addWidget(rootCb);
  container->setLayout(vbox);
  setCentralWidget(container);

  setMinimumWidth(500);
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
  show();
  setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(),
                                  qApp->primaryScreen()->geometry()));
  connect(edit, SIGNAL(returnPressed()), this, SLOT(doExec()));
  connect(edit, SIGNAL(textChanged(const QString &)), this,
          SLOT(resetStatusBar(const QString &)));
  connect(rootCb, SIGNAL(clicked()), this, SLOT(rootCbClicked()));
  initHistory();
  initAutoCompleter();
}

void MainWin::rootCbClicked() { edit->setFocus(); }

void MainWin::resetStatusBar(const QString & /*unused*/) {
  statusMsg->setText("");
}

void MainWin::doExec() {
  auto encoder{QStringEncoder(QStringEncoder::Utf8)};
  QByteArray enccmd{encoder(edit->text())};

  if (*enccmd.data() == '\0') return;
  addToHistory(edit->text());
  if (rootCb->checkState() == Qt::Checked) {
    QByteArray encmsg{
        encoder(tr("Execute command '%1' as root").arg(enccmd.data()))};
    dsbexec_exec(true, enccmd.data(), encmsg.data());
  } else
    dsbexec_exec(false, enccmd.data(), NULL);
  if ((dsbexec_error() & DSBEXEC_ENOENT))
    statusMsg->setText(tr("Command not found"));
  else if ((dsbexec_error() & DSBEXEC_EUNTERM))
    statusMsg->setText(tr("Unterminated quoted string"));
  else
    qh::err(this, EXIT_FAILURE, "dsbexec_exec()");
}

void MainWin::closeEvent(QCloseEvent * /* unused */) {
  QCoreApplication::exit(0);
}

void MainWin::initAutoCompleter() {
  char *p, *paths;
  QStringList list;

  if ((p = getenv("PATH")) == NULL) return;
  if ((paths = strdup(p)) == NULL) qh::err(this, EXIT_FAILURE, "strdup()");
  for (p = paths; (p = strtok(p, ":")) != NULL; p = NULL) {
    QDir dir(p);
    list += dir.entryList(QStringList(), QDir::Files);
    list.sort();
  }
  free(paths);
  QCompleter *completer = new QCompleter(list, this);
  completer->setCaseSensitivity(Qt::CaseSensitive);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  edit->setCompleter(completer);
}

void MainWin::keyPressEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Escape)
    QCoreApplication::exit(0);
  else if (e->key() == Qt::Key_Up) {
    if (histCursor < history->count() - 1)
      edit->setText(history->at(++histCursor));
    e->ignore();
  } else if (e->key() == Qt::Key_Down) {
    if (histCursor > 0) {
      edit->setText(history->at(--histCursor));
    } else {
      histCursor = -1;
      edit->setText("");
    }
    e->ignore();
  }
}

void MainWin::initHistory() {
  char **hv;
  size_t size;

  histCursor = -1;
  history = new QStringList;

  if ((hv = dsbexec_read_history(&size)) == NULL) {
    if (dsbexec_error() != 0)
      qh::errx(nullptr, EXIT_FAILURE, dsbexec_strerror());
    return;
  }
  for (size_t i = 0; i < size; i++) {
    QString s(hv[i]);
    history->append(s);
  }
}

void MainWin::addToHistory(QString s) {
  history->prepend(s);
  histCursor = -1;
}
