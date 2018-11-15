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
#include <ctype.h>
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
#define MAXARGS		256
#define MAXHISTSIZE	100
#define FATAL_SYSERR	(DSBEXEC_ERR_SYS | DSBEXEC_ERR_FATAL)
#define PATH_HISTORY	".dsbexec.history"

#define ERROR(ret, error, prepend, fmt, ...) do { \
	set_error(error, prepend, fmt, ##__VA_ARGS__); \
	return (ret); \
} while (0)

static int  strtoargv(const char *, char **, size_t, size_t *);
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

int
dsbexec_exec(bool sudo, const char *cmd, const char *msgstr)
{
	int    eflags;
	char   *argv[MAXARGS];
	size_t i, hs, argc;
	
	if (histsize == 0) {
		if (dsbexec_read_history(&hs) == NULL && _error != 0)
			ERROR(-1, 0, true, "dsbexec_read_history()");
	}
	if (sudo) {
		i = 1;
		argv[0] = PATH_DSBSU;
		if (msgstr != NULL && *msgstr != '\0') {
			i += 2;
			argv[1] = "-m";
			argv[2] = (char *)msgstr;
		}
	} else
		i = 0;
	if (strtoargv(cmd, argv + i, MAXARGS - i - 1, &argc) == -1)
		return (-1);
	argv[argc + i] = NULL;
	if (dsbexec_add_to_history(argv[i]) == -1)
		return (-1);
	if (dsbexec_write_history() == -1)
		return (-1);
	execvp(argv[0], argv);
	eflags = DSBEXEC_ERR_SYS | DSBEXEC_EEXECCMD;
	if (errno == ENOENT)
		eflags |= DSBEXEC_ENOENT;
	set_error(eflags, false, "execvp(%s)", argv[i]);
	for (; i < argc; i++)
		free(argv[i]);
	return (-1);
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
	int	      fd;
	char	      histpath[PATH_MAX], tmpath[PATH_MAX];
	FILE	      *tmp;
	size_t	      i;
	struct passwd *pw;

	_error = 0;
	if ((pw = getpwuid(getuid())) == NULL)
		ERROR(-1, FATAL_SYSERR, false, "getpwuid()");
	(void)snprintf(histpath, sizeof(histpath), "%s/%s", pw->pw_dir,
	    PATH_HISTORY);
	(void)snprintf(tmpath, sizeof(tmpath), "%s/%s.XXXXX", pw->pw_dir,
	    PATH_HISTORY);
	endpwent();
	if ((fd = mkstemp(tmpath)) == -1)
		ERROR(-1, FATAL_SYSERR, false, "mkstemp()");
	if ((tmp = fdopen(fd, "w")) == NULL)
		ERROR(-1, FATAL_SYSERR, false, "fdopen()");
	for (i = 0; i < histsize; i++)
		(void)fprintf(tmp, "%s\n", history[i]);
	(void)fclose(tmp);
	if (rename(tmpath, histpath) == -1) {
		set_error(FATAL_SYSERR, false, "rename(%s, %s)", tmpath,
		    histpath);
		(void)remove(tmpath);
		return (-1);
	}
	return (0);
}

static int
strtoargv(const char *str, char **argv, size_t argvsz, size_t *argc)
{
	int   squote, dquote, esc;
	size_t n;
	char *buf, *p;

	if ((p = buf = strdup(str)) == NULL)
		ERROR(-1, FATAL_SYSERR, false, "strdup()");
	squote = dquote = n = esc = 0;
	for (; n < argvsz && *str != '\0'; str++) {
		if (*str == '"') {
			if (esc) {
				*p++ = *str;
				esc ^= 1;
			} else if (squote)
				*p++ = *str;
			else
				dquote ^= 1;
		} else if (*str == '\'') {
			if (esc) {
				*p++ = *str;
				esc ^= 1;
			} else
				squote ^= 1;
		} else if (*str == '\\') {
			if (squote) {
				*p++ = *str;
			} else if (esc) {
				*p++ = *str;
				esc ^= 1;
			} else
				esc ^= 1;
		} else if (isspace(*str)) {
			if (esc) {
				*p++ = *str;
				esc ^= 1;
			} else if (dquote || squote) {
				*p++ = *str;
			} else {
				*p = '\0';
				if (dquote || squote)
					goto err_unterm;
				while (isspace(str[1]))
					str++;
				argv[n++] = strdup(buf);
				if (argv[n - 1] == NULL) {
					ERROR(-1, FATAL_SYSERR, false,
					    "strdup()");
				}
				p = buf;
			}
		} else
			*p++ = *str;
	}
	*p = '\0';
	if (dquote || squote)
		goto err_unterm;
	if (p != buf) {
		argv[n++] = strdup(buf);
		if (argv[n - 1] == NULL)
			ERROR(-1, FATAL_SYSERR, false, "strdup()");
	}
	free(buf);
	*argc = n;

	return (0);
err_unterm:
	while (n-- > 0)
		free(argv[n]);
	free(buf);
	ERROR(-1, DSBEXEC_EUNTERM, false, "Unterminated quoted string");
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
		_error += _errno;
		len = strlen(errmsg);
		(void)snprintf(errmsg + len, sizeof(errmsg) - len,
		    ": %s", strerror(_errno));
		errno = 0;
	}
}

