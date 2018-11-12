/*-
 * Copyright (c) 2018 Marcel Kaiser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QDesktopWidget>
#include <QTextCodec>
#include <QDir>
#include <QDebug>
#include <QStringList>
#include <QString>
#include "mainwin.h"
#include "qt-helper/qt-helper.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent) {
	QString prompt	    = QString(tr("Command:"));
	QIcon okIcon	    = qh_loadStockIcon(QStyle::SP_DialogOkButton, 0);
	QIcon cancelIcon    = qh_loadStockIcon(QStyle::SP_DialogCancelButton,
					       NULL);
	QIcon pic	    = qh_loadIcon("system-run", NULL);
	rootCb		    = new QCheckBox(tr("Execute as root"));
	edit	    	    = new QLineEdit(this);
	QLabel	    *icon   = new QLabel(this);	      

	QLabel	    *label  = new QLabel(prompt);
	QPushButton *ok	    = new QPushButton(okIcon, tr("&Ok"));
	QPushButton *cancel = new QPushButton(cancelIcon, tr("&Cancel"));
	QVBoxLayout *evbox  = new QVBoxLayout;
	QVBoxLayout *vbox   = new QVBoxLayout;
	QHBoxLayout *bbox   = new QHBoxLayout;
	QHBoxLayout *hbox   = new QHBoxLayout;
	QWidget *container  = new QWidget(this);

	icon->setPixmap(pic.pixmap(64));
	label->setStyleSheet("font-weight: bold;");

	bbox->addWidget(ok,     1, Qt::AlignRight);
        bbox->addWidget(cancel, 0, Qt::AlignRight);

	evbox->addWidget(label, 1, Qt::AlignLeft);
	evbox->addWidget(edit);

	hbox->addWidget(icon, 0, Qt::AlignLeft);
	hbox->addLayout(evbox);

	vbox->addLayout(hbox);
	vbox->addWidget(rootCb);
	vbox->addLayout(bbox);
	container->setLayout(vbox);
	setCentralWidget(container);

	setMinimumWidth(500);
	setMaximumWidth(500);
	setWindowIcon(pic);
	setWindowTitle(tr("DSBExec - Execute command"));
	show();
	setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
	    size(), qApp->desktop()->availableGeometry()));
	connect(ok, SIGNAL(clicked()), this, SLOT(doExec()));
	connect(edit, SIGNAL(returnPressed()), this, SLOT(doExec()));
	connect(cancel, SIGNAL(clicked()), this, SLOT(cbCancel()));
	initAutoCompleter();
}

void
MainWin::doExec()
{
	char	   *str;
	QTextCodec *codec = QTextCodec::codecForLocale();
	QByteArray encstr = codec->fromUnicode(edit->text());
	const char *input = encstr.data();
	
	if (*input == '\0')
		return;
	if ((cmdstr = str = strdup(input)) == NULL)
		qh_err(this, EXIT_FAILURE, "strdup()");
	if (rootCb->checkState() == Qt::Checked) {
		encstr = codec->fromUnicode(
		    tr("%s -m \"Execute command '%s' as root\" \"%s\""));
		char *msg = strdup(encstr.data());
		if (msg == NULL)
			qh_err(this, EXIT_FAILURE, "strdup()");
		size_t len = strlen(msg) + 2 * strlen(str) +
			     strlen(PATH_DSBSU) + 1;
		if ((cmdstr = (char *)malloc(len)) == NULL)
			qh_err(this, EXIT_FAILURE, "malloc()");
		(void)snprintf(cmdstr, len, msg, PATH_DSBSU, str, str);
		free(msg); free(str);
		proc = dsbexec_exec(cmdstr);
	} else
		proc = dsbexec_exec(cmdstr);
	close();
	if (proc == NULL)
		QCoreApplication::exit(1);
	QCoreApplication::exit(0);
}

void
MainWin::cbCancel()
{
	QCoreApplication::exit(-1);
}

void MainWin::closeEvent(QCloseEvent * /* unused */)
{
	QCoreApplication::exit(-1);
}

void MainWin::initAutoCompleter()
{
	char	    *p, *paths;
	QStringList list;

	if ((p = getenv("PATH")) == NULL)
		return;
	if ((paths = strdup(p)) == NULL)
		qh_err(this, EXIT_FAILURE, "strdup()");
	for (p = paths; (p = strtok(p, ":")) != NULL; p = NULL) {
		QDir dir(p);
		list += dir.entryList(QStringList(), QDir::Files);
		list.sort();
	}
	free(paths);

	autoCompleter = new QCompleter(list, this);
	autoCompleter->setCaseSensitivity(Qt::CaseSensitive);
	autoCompleter->setCompletionMode(QCompleter::PopupCompletion);
	edit->setCompleter(autoCompleter);
}
