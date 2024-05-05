/*-
 * Copyright (c) 2024 Marcel Kaiser. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#ifndef _DSBEXEC_H_
#define _DSBEXEC_H_
#include <stdbool.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include "defs.h"

#ifndef PATH_DSBSU
#define PATH_DSBSU "dsbsu"
#endif

#define DSBEXEC_ERR_FATAL (1 << 7)
#define DSBEXEC_EEXECCMD (1 << 8)
#define DSBEXEC_ERR_SYS (1 << 9)
#define DSBEXEC_EUNTERM (1 << 10)
#define DSBEXEC_ENOENT (1 << 11)

__BEGIN_DECLS
extern int dsbexec_error(void);
extern int dsbexec_write_history(void);
extern int dsbexec_add_to_history(const char *);
extern int dsbexec_exec(bool, const char *, const char *);
extern bool dsbexec_running(void);
extern char **dsbexec_read_history(size_t *);
extern const char *dsbexec_strerror(void);
__END_DECLS

#endif /* !_DSBEXEC_H_ */
