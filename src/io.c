/* io.c
 *
 */

#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "io.h"
#include "werror.h"
#include "write_buffer.h"
#include "xalloc.h"

/* A little more than an hour */
#define MAX_TIMEOUT 4000

struct fd_read
{
  struct abstract_read a;
  int fd;
};

static int do_read(struct fd_read *closure, UINT8 *buffer, UINT32 length)
{
  while(1)
    {
      int res = read(closure->fd, buffer, length);
      if (!res)
	return A_EOF;
      if (res > 0)
	return res;
      
      switch(errno)
	{
	case EINTR:
	  continue;
	case EWOULDBLOCK:  /* aka EAGAIN */
	  return 0;
	default:
	  werror("io.c: do_read: read() failed, %s\n", strerror(errno));
	  return A_FAIL;
	}
    }
}

#define FOR_FDS(type, fd, list, extra)				\
{								\
  type **(_fd);							\
  type *(fd);							\
  for(_fd = &(list); ((fd) = *_fd); _fd = &(*_fd)->next, (extra))	


#define END_FOR_FDS } 
     
#define UNLINK_FD (*_fd = (*_fd)->next)
    
void io_run(struct io_backend *b)
{
  while(1)
    {
      struct pollfd *fds;
      int i;
      nfds_t nfds;
      int timeout;
      int res;
      
      nfds = b->nio + b->nlisten + b->nconnect;

      if (b->callouts)
	{
	  time_t now = time(NULL);
	  if (now >= b->callouts->when)
	    timeout = 0;
	  else
	    {
	      if (b->callouts->when > now + MAX_TIMEOUT)
		timeout = MAX_TIMEOUT * 1000;
	      else
		timeout = (b->callouts->when - now) * 1000;
	    }
	}
      else
	{
	  if (!nfds)
	    /* All done */
	    break;
	  timeout = -1;
	}
      
      fds = alloca(sizeof(struct pollfd) * nfds);

      /* Handle fds in order: read, accept, connect, write, */
      i = 0;

      FOR_FDS(struct io_fd, fd, b->io, i++)
	{
	  fds[i].fd = fd->on_hold ? -1 : fd->fd;
	  fds[i].events = 0;
	  if (!fd->on_hold)
	    fds[i].events |= POLLIN;

	  /* pre_write returns 0 if the buffer is empty */
	  if (write_buffer_pre_write(fd->buffer))
	    fds[i].events |= POLLOUT;
	}
      END_FOR_FDS;

      FOR_FDS(struct listen_fd, fd, b->listen, i++)
	{
	  fds[i].fd = fd->fd;
	  fds[i].events = POLLIN;
	}
      END_FOR_FDS;

      FOR_FDS(struct connect_fd, fd, b->connect, i++)
	{
	  fds[i].fd = fd->fd;
	  fds[i].events = POLLOUT;
	}
      END_FOR_FDS;

      res = poll(fds, nfds, timeout);

      if (!res)
	{
	  /* Timeout. Run the callout */
	  struct callout *f = b->callouts;
	  
	  if (!CALLBACK(f->callout))
	    fatal("What now?");
	  b->callouts = f->next;
	  free(f);
	}
      if (res<0)
	{
	  switch(errno)
	    {
	    case EAGAIN:
	    case EINTR:
	      continue;
	    default:
	      fatal("io_run:poll failed: %s", strerror(errno));
	    }
	}
      else
	{ /* Process files */
	  i = 0;

	  /* Handle writing first */
	  FOR_FDS(struct io_fd, fd, b->io, i++)
	    {
	      if (fds[i].revents & POLLOUT)
		{
		  UINT32 size = MIN(fd->buffer->end - fd->buffer->start,
				    fd->buffer->block_size);
		  int res = write(fd->fd,
				  fd->buffer->buffer + fd->buffer->start,
				  size);
		  if (!res)
		    fatal("Closed?");
		  if (res < 0)
		    switch(errno)
		      {
		      case EINTR:
		      case EAGAIN:
			break;
		      default:
			CALLBACK(fd->close_callback);
			/* FIXME: Must do this later. Perhaps add a
			 * closed flag to th io_fd struct? */
			fd->please_close = 1;

			break;
		      }
		  else
		    fd->buffer->start += res;
		}
	    }
	  END_FOR_FDS;

	  /* Handle reading */
	  i = 0; /* Start over */
	  FOR_FDS(struct io_fd, fd, b->io, i++)
	    {
	      if (!fd->please_close
		  && (fds[i].revents & POLLIN))
		{
		  struct fd_read r =
		  { { (abstract_read_f) do_read }, fd->fd };

		  /* The handler function returns a new handler for the
		   * file, or NULL. */
		  if (!(fd->handler
			= READ_HANDLER(fd->handler,
				       (struct abstract_read *) &r)))
		    {
		      fd->please_close = 1;
		    }
		}
	      if (fd->please_close)
		{
		  /* FIXME: Cleanup properly...
		   *
		   * After a write error, read state must be freed,
		   * and vice versa. */
		  UNLINK_FD;
		  free(fd->buffer);
		  free(fd);
		}
	    }
	  END_FOR_FDS;

	  FOR_FDS(struct listen_fd, fd, b->listen, i++)
	    {
	      if (fds[i].revents & POLLIN)
		{
		  /* FIXME: Do something with the peer address? */
		  struct sockaddr_in peer;
		  
		  int conn = accept(fd->fd, &peer, sizeof(peer));
		  if (conn < 0)
		    {
		      werror("io.c: accept() failed, %s", strerror(errno));
		      continue;
		    }
		  if (!FD_CALLBACK(fd->callback, conn))
		    {
		      /* FIXME: Should fd be closed here? */
		      UNLINK_FD;
		      free(fd);
		    }
		}
	    }
	  END_FOR_FDS;
	  
	  FOR_FDS(struct connect_fd, fd, b->connect, i++)
	    {
	      if (fds[i]->revents & POLLOUT)
		{
		  if (!FD_CALLBACK(fd->callback, fd->fd))
		    fatal("What now?");
		  UNLINK_FD;
		  free(fd);
		}
	    }
	  END_FOR_FDS;
	}
    }
}

/*
 * Fill in ADDR from HOST, SERVICE and PROTOCOL.
 * Supplying a null pointer for HOST means use INADDR_ANY.
 * Otherwise HOST is an numbers-and-dits ip-number or a dns name.
 *
 * PROTOCOL can be tcp or udp.
 *
 * Supplying a null pointer for SERVICE, means use port 0, i.e. no port.
 * 
 * Returns zero on errors, 1 if everything is ok.
 */
int
get_inaddr(struct sockaddr_in	* addr,
	   const char		* host,
	   const char		* service,
	   const char		* protocol)
{
  memset(addr, 0, sizeof *addr);
  addr->sin_family = AF_INET;

  /*
   *  Set host part of ADDR
   */
  if (host == NULL)
    addr->sin_addr.s_addr = INADDR_ANY;
  else
    {
      /* First check for numerical ip-number */
      addr->sin_addr.s_addr = inet_addr(host);
      if (addr->sin_addr.s_addr == (unsigned long)-1)
	{
	  struct hostent * hp;
	  
	  hp = gethostbyname(host);
	  if (hp == NULL)
	    return 0;
	  memcpy(&addr->sin_addr, hp->h_addr, hp->h_length);
	  addr->sin_family = hp->h_addrtype;
	}
    }

  /*
   *  Set port part of ADDR
   */
  if (service == NULL)
    addr->sin_port = htons(0);
  else
    {
      char		* end;
      long		  portno;

      portno = strtol(service, &end, 10);
      if (portno > 0  &&  portno <= 65535
	  &&  end != service  &&  *end == '\0')
	{
	  addr->sin_port = htons(portno);
	}
      else
	{
	  struct servent	* serv;

	  serv = getservbyname(service, "tcp");
	  if (serv == NULL)
	    return 0;
	  addr->sin_port = serv->s_port;
	}
    }

  return 1;
}

void io_set_nonblocking(int fd)
{
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    fatal("io_set_nonblocking: fcntl() failed, %s", strerror(errno));
}

/* Some code is taken from bellman's tcputils. */
struct connect_fd *io_connect(struct io_backend *b,
			      struct sockaddr_in *remote,
			      struct sockaddr_in *local,
			      struct fd_callback *f)
{
  struct connect_fd *fd;
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  if (s<0)
    return NULL;

  io_set_nonblocking(s);

  if (local  &&  bind(s, (struct sockaddr *)local, sizeof *local) < 0)
    {
      int saved_errno = errno;
      close(s);
      errno = saved_errno;
      return NULL;
    }

  if ( (connect(s, (struct sockaddr *)remote, sizeof *remote) < 0)
       && (errno != EINPROGRESS) )       
    {
      int saved_errno = errno;
      close(s);
      errno = saved_errno;
      return NULL;
    }
  
  fd = xalloc(sizeof(struct connect_fd));
  fd->fd = s;
  fd->callback = callback;

  fd->next = b->connect;
  b->connect = fd;

  b->nconnect++;
  
  return fd;
}

struct listen_fd *io_listen(struct io_backend *b,
			    struct sockaddr_in *local,
			    struct fd_callback *callback)
{
  struct listen_fd *fd;
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  if (s<0)
    return NULL;

  io_set_nonblocking(s);

  {
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof yes);
  }

  if (bind(s, (struct sockaddr *)local, sizeof *local) < 0)
    {
      close(s);
      return NULL;
    }

  if (listen(s, 256) < 0) 
    {
      close(s);
      return NULL;
    }

  fd = xalloc(sizeof(struct connect_fd));

  fd->fd = s;
  fd->callback = callback;

  fd->next = b->listen;
  b->listen = fd;
  b->nlisten++;
  
  return fd;
}

struct abstract_write *io_read_write(struct io_backend *b,
				     int fd,
				     struct read_callback *read_callback,
				     UINT32 block_size,
				     struct callback *close_callback)
{
  struct io_fd = xalloc(sizeof(struct io_fd));
  struct write_buffer *buffer = write_buffer_alloc(block_size);
  
  fd->fd = fd;
  fd->please_close = 0;
  
  fd->read_callback = read_callback;
  fd->close_callback = close_callback;
  fd->buffer = buffer;

  fd->next = b->io;
  b->io = fd;
  b->nio++;

  return (struct abstract_write *) buffer;
}
