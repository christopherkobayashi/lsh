/* exception.c
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include "exception.h"

#include "io.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#define GABA_DEFINE
#include "exception.h.x"
#undef GABA_DEFINE

#if 0
static void
do_default_handler(struct exception_handler *ignored UNUSED,
		   const struct exception *e)
{
  fatal("Unhandled exception of type 0x%xi: %z\n", e->type, e->msg);
}

struct exception_handler default_exception_handler =
STATIC_EXCEPTION_HANDLER(do_default_handler, NULL);
#endif

DEFINE_EXCEPTION_HANDLER(ignore_exception_handler)
     (struct exception_handler *self UNUSED,
      const struct exception *e)
{
  trace("Ignoring exception: %z (type %i:%i)\n",
	e->msg, e->type, e->subtype);
}

#if 0
struct exception_handler *
make_exception_handler(void (*raise)(struct exception_handler *s,
				     const struct exception *x),
		       struct exception_handler *parent,
		       const char *context)
{
  NEW(exception_handler, self);
  self->raise = raise;
  self->parent = parent;
  self->context = context;
  
  return self;
}

struct report_exception_info *
make_report_exception_info(uint32_t mask, uint32_t value,
			   const char *prefix)
{
  NEW(report_exception_info, self);

  self->mask = mask;
  self->value = value;
  self->prefix = prefix;

  return self;
}

/* ;;GABA:
   (class
     (name report_exception_handler)
     (super exception_handler)
     (vars
       (info const object report_exception_info)))
*/

static void
do_report_exception_handler(struct exception_handler *s,
			    const struct exception *x)
{
  CAST(report_exception_handler, self, s);
  const struct report_exception_info *info = self->info;
  
  if ( (x->type & info->mask) == info->value)
    werror("%z exception: %z\n", info->prefix, x->msg);
  else
    EXCEPTION_RAISE(self->super.parent, x);
}

struct exception_handler *
make_report_exception_handler(const struct report_exception_info *info,
			      struct exception_handler *parent,
			      const char *context)
{
  NEW(report_exception_handler, self);
  self->super.raise = do_report_exception_handler;
  self->super.parent = parent;
  self->super.context = context;

  self->info = info;

  return &self->super;
}
#endif

struct exception *
make_exception(int type, int subtype, const char *msg)
{
  NEW(exception, e);
  e->type = type;
  e->subtype = subtype;
  e->msg = msg;

  return e;
}

#if 0
/* Reason == 0 means disconnect without sending any disconnect
 * message. */

struct exception *
make_protocol_exception(uint32_t reason, const char *msg)
{
  NEW(protocol_exception, self);

#define MAX_REASON 15
  const char *messages[MAX_REASON+1] =
  {
    NULL, "Host not allowed to connect",
    "Protocol error", "Key exchange failed",
    "Host authentication failed", "MAC error",
    "Compression error", "Service not available",
    "Protocol version not supported", "Host key not verifiable",
    "Connection lost", "By application",
    "Too many connections", "Auth cancelled by user",
    "No more auth methods available", "Illegal user name"
  };
    
  assert(reason <= MAX_REASON);
#undef MAX_REASON

  self->super.type = EXC_PROTOCOL;
  self->super.msg = msg ? msg : messages[reason];

  self->reason = reason;

  return &self->super;
}
#endif

#if DEBUG_TRACE
void
exception_raise(struct exception_handler *h,
		const struct exception *e,
		const char *context)
{
  trace ("%z: Raising exception %z (type %i:%i), using handler installed by %z\n",
	 context, e->msg, e->type, e->subtype, h->context);
  h->raise(h, e);
}
#endif /* DEBUG_TRACE */
