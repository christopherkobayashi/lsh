/*
 *
 * rl.h
 */

/* lsftp, an implementation of the sftp protocol
 *
 * Copyright (C) 2001, Pontus Sköld
 * Portions of this code originately from the readline manual
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

#ifndef LSFTP_RL_H
#define LSFTP_RL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "commands.h"
#include "str_utils.h"
#include "misc_fun.h"


/* Ehrm */
#define REMOTEMAXLEN 256             

#ifdef HAVE_READLINE_READLINE_H
/* Use readline */
# include <readline/readline.h>
#else
# ifdef HAVE_READLINE_H
#  include <readline.h>
# endif
#endif /* HAVE_READLINE_READLINE_H */

#ifdef  HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#else
# ifdef HAVE_HISTORY_H
#  include <history.h>
# endif
#endif /* HAVE_READLINE_HISTORY_H */

#define LSFTP_HISTNAME_VAR "LSFTP_HISTORY_FILE"
#define LSFTP_HIST_FILENAME "/.lsftp"







/* These are "public" functions */

void lsftp_rl_init_get_line(char* prompt);
char* lsftp_rl_get_line(void );
void lsftp_rl_check_input(void);
void lsftp_rl_unuse_line(char* line);
void lsftp_rl_init(void);
void lsftp_rl_exit(void);
char* lsftp_rl_history_fname(void);

/* "Private" functions below */

void lsftp_rl_lhandler(char* line);

rl_completion_func_t lsftp_rl_completion;
rl_compentry_func_t lsftp_rl_command_generator;
rl_compentry_func_t lsftp_rl_remotefile_generator;
rl_compentry_func_t lsftp_rl_no_generator;

rl_linebuf_func_t char_quoted;

extern int interactive;

#endif /* LSFTP_RL_H */
