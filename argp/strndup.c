/* strndup.c
 *
 */

/* Written by Niels Möller <nisse@lysator.liu.se>
 *
 * This file is hereby placed in the public domain.
 */

#include <stdlib.h>
#include <string.h>

char *
strndup (const char *s, size_t size)
{
  char *r;
  char *end = memchr(s, 0, size);
  
  if (end)
    /* strlen, i.e., excluding the terminating NUL. */
    size = end - s;
  
  r = malloc(size+1);

  if (r)
    {
      memcpy(r, s, size);
      r[size] = '\0';
    }
  return r;
}
