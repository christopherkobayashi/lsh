/*
 * @(#) $Id$
 *
 * sftp_c.h
 *
 * Portions of code taken from the sftp test client from
 * the sftp server of lsh by Niels M�ller and myself.
 *
 */

/* lsftp, an implementation of the sftp protocol
 *
 * Copyright (C) 2001 Pontus Sk�ld
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

#ifndef SFTP_C_H
#define SFTP_C_H

#define SFTP_VERSION 3
#define SFTP_BLOCKSIZE 16384

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "sftp.h"
#include "buffer.h"
#include "xmalloc.h"



struct sftp_mem {
  char* at;
  int size;
  int used;
};


struct sftp_callback;

struct sftp_callback {
  struct sftp_callback (*nextfun) (
				   UINT8 msg, 
				   UINT32 id,
				   struct sftp_input *in,
				   struct sftp_output *out,
  				   struct sftp_callback state 
  				   );
  
  UINT32 id; /* Id - for which id is this callback valid? */
  
  UINT64 filepos; 
  UINT64 filesize;  /* Only used for informational purposes */

  int op_id; /* Operation ID - identifier for caller */
  
  int fd;
  UINT32 retval; /* Return value (if id & nextfun == NULL ) of LAST call */
  int last; /* What was the last callback? */
  
  UINT32 bad_status; /* Return value of FAILED operation */
  UINT32 last_bad;   /* Who got FAILED status */
  
  int localerr; /* Return value when we had an local error */
  int localerrno; /* errno */

  struct sftp_attrib attrib;
  struct sftp_mem mem;  /* Memory buffer */
  
  UINT8 *handle;      /* Handle string from open and opendir calls */
  UINT32 handlelen; 
};


struct sftp_callback sftp_null_state();
struct sftp_mem sftp_alloc_mem( int desired_size );  /* Allocates a string/buffer of the given size */
int sftp_free_mem( struct sftp_mem *mem );           /* Free that buffer */ 
int sftp_resize_mem( struct sftp_mem *mem, int newsize ); /* */
struct sftp_mem sftp_null_mem();                          /* Suitable for new */
int sftp_toggle_mem( struct sftp_mem *mem );


int sftp_store( struct sftp_mem* mem, void* data, UINT32 datalen );
void* sftp_retrieve( struct sftp_mem *mem, UINT32 desired, UINT32* realsize );

UINT32 sftp_rumask( UINT32 new );

void sftp_attrib_from_stat( struct stat *st, struct sftp_attrib* a );
mode_t sftp_attrib_perms( struct sftp_attrib* a);

UINT32 sftp_unique_id();


struct sftp_callback sftp_symlink_init(
				      int op_id,
				      struct sftp_input *in, 
				      struct sftp_output *out,
				      UINT8 *linkname, 
				      UINT32 linklen,
				      UINT8 *targetname, 
				      UINT32 targetlen
				      );



struct sftp_callback sftp_rename_init(
				      int op_id,
				      struct sftp_input *in, 
				      struct sftp_output *out,
				      UINT8 *srcname, 
				      UINT32 srclen,
				      UINT8 *dstname, 
				      UINT32 dstlen
				      );



struct sftp_callback sftp_remove_init(
				      int op_id,
				      struct sftp_input *in, 
				      struct sftp_output *out,
				      UINT8 *name,
				      UINT32 namelen
				      );

struct sftp_callback sftp_mkdir_init(
				     int op_id,
				     struct sftp_input *in, 
				     struct sftp_output *out,
				     UINT8 *name,
				     UINT32 namelen,
				     struct sftp_attrib* a
				     );

struct sftp_callback sftp_rmdir_init(
				     int op_id,
				     struct sftp_input *in, 
				     struct sftp_output *out,
				     UINT8 *name,
				     UINT32 namelen
				     );


struct sftp_callback sftp_realpath_init(
					int op_id,
					struct sftp_input *in, 
					struct sftp_output *out,
					UINT8 *name,
					UINT32 namelen
					);

struct sftp_callback sftp_readlink_init(
					int op_id,
					struct sftp_input *in, 
					struct sftp_output *out,
					UINT8 *name,
					UINT32 namelen
					);


struct sftp_callback sftp_stat_init(
				    int op_id,
				    struct sftp_input *in, 
				    struct sftp_output *out,
				    UINT8 *name,
				    UINT32 namelen
				    );


struct sftp_callback sftp_lstat_init(
				     int op_id,
				     struct sftp_input *in, 
				     struct sftp_output *out,
				     UINT8 *name,
				     UINT32 namelen
				     );

struct sftp_callback sftp_fstat_init(
				     int op_id,
				     struct sftp_input *in, 
				     struct sftp_output *out,
				     UINT8 *handle,
				     UINT32 handlelen
				     );


struct sftp_callback sftp_setstat_init(
				       int op_id,
				       struct sftp_input *in, 
				       struct sftp_output *out,
				       UINT8 *name,
				       UINT32 namelen,
				       struct sftp_attrib* attrib
				       );


struct sftp_callback sftp_fsetstat_init(
					int op_id,
					struct sftp_input* in, 
					struct sftp_output* out,
					UINT8 *handle,
					UINT32 handlelen,
					struct sftp_attrib* attrib
					);



/* Get to memory */

struct sftp_callback sftp_get_mem_init(
				       int op_id,
				       struct sftp_input *in,
				       struct sftp_output *out,
				       UINT8 *name, 
				       UINT32 namelen,
				       struct sftp_mem *mem,
				       UINT64 startat
				       );

struct sftp_callback sftp_get_mem_step_one(
					   UINT8 msg,
					   UINT32 id,
					   struct sftp_input *in,
					   struct sftp_output *out,
					   struct sftp_callback state
					   );


struct sftp_callback sftp_get_mem_main(
				       UINT8 msg,
				       UINT32 id,
				       struct sftp_input *in,
				       struct sftp_output *out,
				       struct sftp_callback state
				       );

/* End get to memory */

/* Get to file */

struct sftp_callback sftp_get_file_init(
					int op_id,
					struct sftp_input *in,
					struct sftp_output *out,
					UINT8 *name, 
					UINT32 namelen,
					UINT8 *fname, 
					UINT32 fnamelen,
					int cont
					);

struct sftp_callback sftp_get_file_step_one(
					    UINT8 msg,
					    UINT32 id,
					    struct sftp_input *in,
					    struct sftp_output *out,
					    struct sftp_callback state
					    );

struct sftp_callback sftp_get_file_step_two(
					    UINT8 msg,
					    UINT32 id,
					    struct sftp_input *in,
					    struct sftp_output *out,
					    struct sftp_callback state
					    );


struct sftp_callback sftp_get_file_main(
					UINT8 msg,
					UINT32 id,
					struct sftp_input *in,
					struct sftp_output *out,
					struct sftp_callback state
					);

/* End get to file */

/* Put fromfile */

struct sftp_callback sftp_put_file_init(
					int op_id,
					struct sftp_input *in,
					struct sftp_output *out,
					UINT8 *name,
					UINT32 namelen,
					UINT8 *fname,
					UINT32 fnamelen,
					int cont
					);

struct sftp_callback sftp_put_file_step_one(
					    UINT8 msg,
					    UINT32 id,
					    struct sftp_input *in,
					    struct sftp_output *out,
					    struct sftp_callback state
					    );


struct sftp_callback sftp_put_file_main(
					UINT8 msg,
					UINT32 id,
					struct sftp_input *in,
					struct sftp_output *out,
					struct sftp_callback state
					);


struct sftp_callback sftp_put_file_do_fstat(					
					    UINT8 msg,
					    UINT32 id,
					    struct sftp_input *in,
					    struct sftp_output *out,
					    struct sftp_callback state
					    );

struct sftp_callback sftp_put_file_handle_stat(
					       UINT8 msg,
					       UINT32 id,
					       struct sftp_input *in,
					       struct sftp_output *out,
					       struct sftp_callback state
					       );

/* End put from file */



/* Put from memory */

struct sftp_callback sftp_put_mem_init(
				       int op_id,
				       struct sftp_input *in,
				       struct sftp_output *out,
				       UINT8 *name, 
				       UINT32 namelen,
				       struct sftp_mem *mem,
				       UINT64 startat,
				       struct sftp_attrib a
				       );

struct sftp_callback sftp_put_mem_step_one(	
					   UINT8 msg,
					   UINT32 id,
					   struct sftp_input *in,
					   struct sftp_output *out,
					   struct sftp_callback state
					   );


struct sftp_callback sftp_put_mem_main(
				       UINT8 msg,
				       UINT32 id,
				       struct sftp_input *in,
				       struct sftp_output *out,
				       struct sftp_callback state
				       );

/* End put from memory */


struct sftp_callback sftp_ls_init(			
				  int op_id,
				  struct sftp_input *in,
				  struct sftp_output *out,
				  UINT8 *dir,
				  UINT32 dirlen
				  );

struct sftp_callback sftp_ls_step_one(
				      UINT8 msg,
				      UINT32 id,
				      struct sftp_input *in,
				      struct sftp_output *out,
				      struct sftp_callback state
				      );



struct sftp_callback sftp_ls_main(
				  UINT8 msg,
				  UINT32 id,
				  struct sftp_input *in,
				  struct sftp_output *out,
				  struct sftp_callback state
				  );

struct sftp_callback sftp_handle_status(
					UINT8 msg,
					UINT32 id,
					struct sftp_input *in, 
					struct sftp_output *out,
					struct sftp_callback state
					);

struct sftp_callback sftp_handle_attrs(
				       UINT8 msg,
				       UINT32 id,
				       struct sftp_input *in, 
				       struct sftp_output *out,
				       struct sftp_callback state
				       );


struct sftp_callback sftp_handle_name(
				      UINT8 msg,
				      UINT32 id,
				      struct sftp_input *in, 
				      struct sftp_output *out,
				      struct sftp_callback state
				      );

int sftp_handshake( 
		   struct sftp_input *in,
		   struct sftp_output *out
		   );



enum sftp_last {
  SFTP_HANDLE_STATUS = 16,
  SFTP_HANDLE_ATTRS = 17,
  SFTP_HANDLE_NAME = 18,
  SFTP_MKDIR_INIT = 19,
  SFTP_RMDIR_INIT = 20,
  SFTP_REALPATH_INIT = 21,
  SFTP_STAT_INIT = 22,
  SFTP_LSTAT_INIT = 23,
  SFTP_FSTAT_INIT = 24,
  SFTP_SETSTAT_INIT = 25,
  SFTP_FSETSTAT_INIT = 26,
  SFTP_GET_MEM_INIT = 27,
  SFTP_GET_FILE_INIT = 28,
  SFTP_GET_FILE_STEP_ONE = 29,
  SFTP_GET_MEM_STEP_ONE = 30,
  SFTP_GET_FILE_MAIN = 31,
  SFTP_GET_MEM_MAIN = 32,
  SFTP_PUT_MEM_INIT = 33,
  SFTP_PUT_FILE_INIT = 34,
  SFTP_PUT_MEM_STEP_ONE = 35,
  SFTP_PUT_FILE_STEP_ONE = 36,
  SFTP_PUT_MEM_MAIN = 37,
  SFTP_PUT_FILE_MAIN = 38,
  SFTP_PUT_FILE_DO_FSTAT = 39,
  SFTP_PUT_FILE_HANDLE_STAT = 40,
  SFTP_LS_MAIN = 41,
  SFTP_GET_FILE_STEP_TWO = 42,
  SFTP_READLINK_INIT = 43,
  SFTP_SYMLINK_INIT = 44,
  SFTP_RENAME_INIT = 45,
  SFTP_REMOVE_INIT = 46,
  SFTP_HANDLE_LAST_UNKNOWN = -1
};

#endif /* SFTP_H */


