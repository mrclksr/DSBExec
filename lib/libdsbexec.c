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

#include <err.h>
#include <errno.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <limits.h>
#include <libutil.h>
#include <time.h>
#include <paths.h>
#include <unistd.h>
#include <signal.h>

#include "libdsbexec.h"

#define ERRBUFSZ	1024
#define MAXHISTSIZE	100
#define FATAL_SYSERR	(DSBEXEC_ERR_SYS | DSBEXEC_ERR_FATAL)
#define PATH_HISTORY	".dsbexec.history"

#define ERROR(ret, error, prepend, fmt, ...) do { \
	set_error(error, prepend, fmt, ##__VA_ARGS__); \
	return (ret); \
} while (0)

struct dsbexec_proc_s {
	int	 pid;
	sigset_t sset;	     /* Saved signal set. */
};

static int  wait_on_proc(dsbexec_proc *);
static void set_error(int, bool, const char *, ...);

static int    _error;
static char   errmsg[ERRBUFSZ];
static char   *history[MAXHISTSIZE];
static size_t histsize;

const char *
dsbexec_strerror()
{
	return (errmsg);
}

int
dsbexec_error()
{
	return (_error);
}

dsbexec_proc *
dsbexec_exec(const char *cmd)
{
	sigset_t     sset;
	dsbexec_proc *proc;

	if ((proc = malloc(sizeof(dsbexec_proc))) == NULL)
		ERROR(NULL, FATAL_SYSERR, false, "malloc()");
	proc->pid = -1;

	(void)sigemptyset(&sset);
        (void)sigaddset(&sset, SIGCHLD);
        (void)sigaddset(&sset, SIGINT);
        (void)sigaddset(&sset, SIGQUIT);
	(void)sigprocmask(SIG_BLOCK, &sset, &proc->sset);
	if ((proc->pid = fork()) == -1) {
		set_error(FATAL_SYSERR, false, "fork()");
		goto error;
	} else if (proc->pid == 0) {
		(void)sigprocmask(SIG_SETMASK, &proc->sset, NULL);
		(void)execl(_PATH_BSHELL, _PATH_BSHELL, "-c", cmd, NULL);
		_exit(DSBEXEC_EEXECSH + errno);
	}
	return (proc);

error:
	(void)sigprocmask(SIG_SETMASK, &proc->sset, NULL);
	free(proc);

	return (NULL);
}

int
dsbexec_wait(dsbexec_proc *proc)
{
	int ret = 0;
	struct sigaction ign, saint, saquit;

	ign.sa_flags   = 0;
	ign.sa_handler = SIG_IGN;
	(void)sigemptyset(&ign.sa_mask);
	(void)sigaction(SIGINT, &ign, &saint);
	(void)sigaction(SIGQUIT, &ign, &saquit);

	/* Wait for the child to terminiate. */
	if (wait_on_proc(proc) != 0)
		ret = -1;
	(void)sigaction(SIGINT, &saint, NULL);
	(void)sigaction(SIGQUIT, &saquit, NULL);
	(void)sigprocmask(SIG_SETMASK, &proc->sset, NULL);

	return (ret);
}

char **
dsbexec_read_history(size_t *size)
{
	FILE	      *fp;
	char	      *line, histpath[PATH_MAX];
	size_t	      linecap;
	struct passwd *pw;

	_error = 0;
	if ((pw = getpwuid(getuid())) == NULL)
		ERROR(NULL, FATAL_SYSERR, false, "getpwuid()");
	(void)snprintf(histpath, sizeof(histpath), "%s/%s", pw->pw_dir,
	    PATH_HISTORY);
	endpwent();
	if ((fp = fopen(histpath, "r")) == NULL) {
		if (errno != ENOENT)
			ERROR(NULL, FATAL_SYSERR, false, "fopen(%s)", histpath);
		return (NULL);
	}
	histsize = linecap = 0; line = NULL;
	while (getline(&line, &linecap, fp) > 0 && histsize < MAXHISTSIZE) {
		if ((history[histsize++] = strdup(line)) == NULL)
			ERROR(NULL, FATAL_SYSERR, false, "strdup()");
		(void)strtok(history[histsize - 1], "\n\r");
	}
	free(line);
	fclose(fp);

	*size = histsize;

	return (history);
}

int
dsbexec_add_to_history(const char *cmd)
{
	size_t i, n;

	_error = 0;
	if (*cmd == '\0')
		return (0);
	if (histsize >= MAXHISTSIZE) {
		histsize = MAXHISTSIZE;
		free(history[histsize - 1]);
		n = histsize - 1;
	} else
		n = histsize++;
	for (i = n; i >= 1;  i--)
		history[i] = history[i - 1];
	if ((history[0] = strdup(cmd)) == NULL)
		ERROR(-1, FATAL_SYSERR, false, "strdup()");
	(void)strtok(history[0], "\n\r");

	return (0);
}

int
dsbexec_write_history()
{
	char	      histpath[PATH_MAX];
	FILE	      *fp;
	size_t	      i;
	struct passwd *pw;

	_error = 0;
	if ((pw = getpwuid(getuid())) == NULL)
		ERROR(-1, FATAL_SYSERR, false, "getpwuid()");
	(void)snprintf(histpath, sizeof(histpath), "%s/%s", pw->pw_dir,
	    PATH_HISTORY);
	endpwent();
	if ((fp = fopen(histpath, "w+")) == NULL)
		ERROR(-1, FATAL_SYSERR, false, "fopen(%s)", histpath);
	for (i = 0; i < histsize; i++)
		(void)fprintf(fp, "%s\n", history[i]);
	fclose(fp);

	return (0);
}

static void
set_error(int error, bool prepend, const char *fmt, ...)
{
	int	_errno;
	char	errbuf[ERRBUFSZ];
	size_t  len;
	va_list ap;

	_errno = errno;
	_error = error;
	va_start(ap, fmt);
	if (prepend) {
		if (error & DSBEXEC_ERR_FATAL) {
			if (strncmp(errmsg, "Fatal: ", 7) == 0) {
				(void)memmove(errmsg, errmsg + 7,
				    strlen(errmsg) - 6);
			}
			(void)strlcpy(errbuf, "Fatal: ", sizeof(errbuf) - 1);
			len = strlen(errbuf);
		} else
			len = 0;
		(void)vsnprintf(errbuf + len, sizeof(errbuf) - len, fmt, ap);
		len = strlen(errbuf);
		(void)snprintf(errbuf + len, sizeof(errbuf) - len, ":%s",
		    errmsg);
		(void)strlcpy(errmsg, errbuf, sizeof(errmsg));
	} else {
		(void)vsnprintf(errmsg, sizeof(errmsg), fmt, ap);
		if (error & DSBEXEC_ERR_FATAL) {
			(void)snprintf(errbuf, sizeof(errbuf), "Fatal: %s",
			    errmsg);
			(void)strlcpy(errmsg, errbuf, sizeof(errmsg));
		}
	}
	if ((error & DSBEXEC_ERR_SYS) && _errno != 0) {
		len = strlen(errmsg);
		(void)snprintf(errmsg + len, sizeof(errmsg) - len,
		    ": %s", strerror(_errno));
		errno = 0;
	}
}

static int
wait_on_proc(dsbexec_proc *proc)
{
	siginfo_t si;

	while (waitid(P_PID, proc->pid, &si, WEXITED | WSTOPPED) == -1) {
		if (errno != EINTR) {
			set_error(FATAL_SYSERR, false, "waitid()");
			return (-1);
		}
	}
	if (si.si_status & DSBEXEC_EEXECSH) {
		/* Failed to execute /bin/sh */
		errno = si.si_status & 0x7f;
		set_error(DSBEXEC_EEXECSH | DSBEXEC_ERR_SYS, false, "exec()");
		return (1);
	} else if (si.si_status & DSBEXEC_ERR_SYS) {
		errno = si.si_status & 0x7f;
		set_error(FATAL_SYSERR, false, "Error");
		return (1);
	} else if (si.si_status == 127) {
		set_error(DSBEXEC_EEXECCMD, false, "Command not found");
		return (1);
	}
	return (0);
}

