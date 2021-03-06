dnl -*- mode: shell-script; sh-indentation: 2; -*-

dnl Process this file with autoconf to produce a configure script.

AC_INIT([libspki], [0.1])
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR([certificate.c])
# Needed to stop autoconf from looking for files in parent directories.
AC_CONFIG_AUX_DIR([.])

AC_CONFIG_HEADER(config.h)

AC_ARG_WITH(include-path,
  AC_HELP_STRING([--with-include-path], [A colon-separated list of directories to search for include files]),,
  [with_include_path=''])

if test x$with_include_path != x ; then
  CPPFLAGS="$CPPFLAGS -I`echo $with_include_path | sed 's/:/ -I/g'`"
fi

AC_ARG_WITH(lib-path,
  AC_HELP_STRING([--with-lib-path], [A colon-separated list of directories to search for libraries]),,
  [with_lib_path=''])

if test x$with_lib_path != x ; then
  LDFLAGS="$LDFLAGS -L`echo $with_lib_path | sed 's/:/ -L/g'`"
fi

LSH_RPATH_INIT([`echo $with_lib_path | sed 's/:/ /g'` \
    `echo $exec_prefix | sed "s@^NONE@$prefix/lib@g" | sed "s@^NONE@$ac_default_prefix/lib@g"` \
    /usr/local/lib /sw/local/lib /sw/lib \
    /usr/gnu/lib /opt/gnu/lib /sw/gnu/lib /usr/freeware/lib /usr/pkg/lib])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

LSH_DEPENDENCY_TRACKING

if test x$enable_dependency_tracking = xyes ; then
  # Since the makefiles use include to get the dependency files, we must
  # make sure that the files exist. We generate some more files than are
  # actually needed.

  AC_CONFIG_COMMANDS([dummy-dep-files],
  [(cd "$srcdir" && find . -name '*.c' -print) \
 | sed 's/\.c$//' | (while read f; do echo > "$f.o.d"; done)
])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

LSH_FUNC_ALLOCA

AC_CHECK_FUNCS([gmtime_r])

# Needed by the supplied memcmp.c
AC_C_BIGENDIAN

LSH_GCC_ATTRIBUTES

# Checks for libraries
AC_CHECK_LIB(gmp, __gmpz_getlimbn,,
    [AC_MSG_ERROR(
    [GNU MP not found, or not 3.1 or up, see http://www.swox.com/gmp.])])

# Add -R flags needed to run programs linked with gmp
LSH_RPATH_FIX

AC_CHECK_LIB(nettle, nettle_sha1_digest,,
    [AC_MSG_ERROR(
    [Nettle library not found. See http://www.lysator.liu.se/~nisse/nettle/.])])

LSH_RPATH_FIX

# Set these flags *last*, or else the test programs won't compile
if test x$GCC = xyes ; then
  # Using -ggdb3 makes (some versions of) Redhat's gcc-2.96 dump core
  if "$CC" --version | grep '^2\.96$' 1>/dev/null 2>&1; then
    true
  else
    CFLAGS="$CFLAGS -ggdb3"
  fi
  # FIXME: It would be better to actually test if this option works and/or is needed.
  if "$CC" --version | grep 'gcc.* 4\.' 1>/dev/null 2>&1; then
    CFLAGS="$CFLAGS -Wno-pointer-sign"
  fi
  CFLAGS="$CFLAGS -Wall -W \
  -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes \
  -Wpointer-arith -Wbad-function-cast -Wnested-externs"
fi

AC_OUTPUT(config.make Makefile tools/Makefile testsuite/Makefile)
