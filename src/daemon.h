/* daemon.h
 *
 * Derived from
 * http://www.zip.com.au/~raf2/lib/software/daemon
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 raf, Niels Möller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301  USA
 */

#ifndef LSH_DAEMON_H_INCLUDED
#define LSH_DAEMON_H_INCLUDED

#include "lsh.h"

enum daemon_mode
{
  DAEMON_NORMAL = 1,
  DAEMON_INIT,
  DAEMON_INETD
};

enum daemon_flags
{
  DAEMON_FLAG_NO_SETSID = 1
};

enum daemon_mode
daemon_detect(void);

int
daemon_dup_null(int fd);

void
daemon_close_fds(void);

int
daemon_disable_core(void);

int
daemon_pidfile(const char *name);
int
daemon_init(enum daemon_mode, enum daemon_flags);
int daemon_close(const char *name);

#endif /* LSH_DAEMON_H_INCLUDED */
