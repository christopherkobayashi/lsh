/* reaper.c
 *
 * Handle child processes.
 *
 * $Id$
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Niels M�ller
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "reaper.h"

#include "alist.h"
#include "werror.h"
#include "xalloc.h"

#include <assert.h>
#include <string.h>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CLASS_DEFINE
#include "reaper.h.x"
#undef CLASS_DEFINE

#include "reaper.c.x"

static sig_atomic_t halloween;

static void child_handler(int signum)
{
  assert(signum == SIGCHLD);

  halloween = 1;
}

/* CLASS:
   (class
     (name reaper)
     (super reap)
     (vars
       ; Mapping of from pids to exit-callbacks. 
       ; NOTE: This assumes that a pid_t fits in an int. 
       (children object alist)))
*/

static void do_reap(struct reap *c,
		    pid_t pid, struct exit_callback *callback)
{
  CAST(reaper, closure, c);

  assert(!ALIST_GET(closure->children, pid));

  ALIST_SET(closure->children, pid, callback);
}
  
static void reap(struct reaper *r)
{
  pid_t pid;
  int status;

  /* We must reset this flag before reaping the zoombies. */
  halloween = 0;
  
  while( (pid = waitpid(-1, &status, WNOHANG)) )
    {
      if (pid > 0)
	{
	  int signaled;
	  int value;
	  int core;
	  struct exit_callback *callback;
	  
	  if (WIFEXITED(status))
	    {
	      verbose("Child %d died with exit code %d.\n",
		      pid, WEXITSTATUS(status));
	      signaled = 0;
	      core = 0;
	      value = WEXITSTATUS(status);
	    }
	  else if (WIFSIGNALED(status))
	    {
	      verbose("Child %d killed by signal %d.\n",
		      pid, WTERMSIG(status));
	      signaled = 1;
	      core = !!WCOREDUMP(status);
	      value = WTERMSIG(status);
	    }
	  else
	    fatal("Child died, but neither WIFEXITED or WIFSIGNALED is true.\n");

	  callback = ALIST_GET(r->children, pid);
	  
	  if (callback)
	    {
	      ALIST_SET(r->children, pid, NULL);
	      EXIT_CALLBACK(callback, signaled, core, value);
	    }
	  else
	    {
	      if (WIFSIGNALED(status))
		werror("Unregistered child %d killed by signal %d.\n",
		       pid, value);
	      else
		werror("Unregistered child %d died with exit status %d.\n",
		       pid, value);
	    }
	}
      else switch(errno)
	{
	case EINTR:
	  werror("reaper.c: waitpid() returned EINTR.\n");
	  break;
	case ECHILD:
	  /* No more child processes */
	  return;
	default:
	  fatal("reaper.c: waitpid failed (errno = %d), %s\n",
		errno, strerror(errno));
	}
    }
}

struct reap *make_reaper(void)
{
  NEW(reaper, closure);

  closure->super.reap = do_reap;
  closure->children = make_linked_alist(0, -1);

  return &closure->super;
}

/* FIXME: Prehaps this function should return a suitable exit code? */
void reaper_run(struct reap *r, struct io_backend *b)
{
  CAST(reaper, self, r);
  
  struct sigaction pipe;
  struct sigaction chld;

  pipe.sa_handler = SIG_IGN;
  sigemptyset(&pipe.sa_mask);
  pipe.sa_flags = 0;
  pipe.sa_restorer = NULL;

  chld.sa_handler = child_handler;
  sigemptyset(&chld.sa_mask);
  chld.sa_flags = SA_NOCLDSTOP;
  chld.sa_restorer = NULL;
  
  if (sigaction(SIGPIPE, &pipe, NULL) < 0)
    fatal("Failed to ignore SIGPIPE.\n");
  if (sigaction(SIGCHLD, &chld, NULL) < 0)
    fatal("Failed to install handler for SIGCHLD.\n");

  halloween = 0;
  while(io_iter(b))
    if (halloween)
      reap(self);
}
