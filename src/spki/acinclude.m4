# LSH_PATH_ADD(path-id, directory)
AC_DEFUN([LSH_PATH_ADD],
[AC_MSG_CHECKING($2)
ac_exists=no
if test -d "$2/." ; then
  ac_real_dir=`cd $2 && pwd`
  if test -n "$ac_real_dir" ; then
    ac_exists=yes
    for old in $1_REAL_DIRS ; do
      ac_found=no
      if test x$ac_real_dir = x$old ; then
        ac_found=yes;
	break;
      fi
    done
    if test $ac_found = yes ; then
      AC_MSG_RESULT(already added)
    else
      AC_MSG_RESULT(added)
      # LDFLAGS="$LDFLAGS -L $2"
      $1_REAL_DIRS="$ac_real_dir [$]$1_REAL_DIRS"
      $1_DIRS="$2 [$]$1_DIRS"
    fi
  fi
fi
if test $ac_exists = no ; then
  AC_MSG_RESULT(not found)
fi
])

# LSH_RPATH_ADD(dir)
AC_DEFUN([LSH_RPATH_ADD], [LSH_PATH_ADD(RPATH_CANDIDATE, $1)])

# LSH_RPATH_INIT(candidates)
AC_DEFUN([LSH_RPATH_INIT],
[AC_MSG_CHECKING([for -R flag])
RPATHFLAG=''
case `uname -sr` in
  OSF1\ V4.*)
    RPATHFLAG="-rpath "
    ;;
  IRIX\ 6.*)
    RPATHFLAG="-rpath "
    ;;
  IRIX\ 5.*)
    RPATHFLAG="-rpath "
    ;;
  SunOS\ 5.*)
    if test "$TCC" = "yes"; then
      # tcc doesn't know about -R
      RPATHFLAG="-Wl,-R,"
    else
      RPATHFLAG=-R
    fi
    ;;
  Linux\ 2.*)
    RPATHFLAG="-Wl,-rpath,"
    ;;
  *)
    :
    ;;
esac

if test x$RPATHFLAG = x ; then
  AC_MSG_RESULT(none)
else
  AC_MSG_RESULT([using $RPATHFLAG])
fi

RPATH_CANDIDATE_REAL_DIRS=''
RPATH_CANDIDATE_DIRS=''

AC_MSG_RESULT([Searching for libraries])

for d in $1 ; do
  LSH_RPATH_ADD($d)
done
])    

# Try to execute a main program, and if it fails, try adding some
# -R flag.
# LSH_RPATH_FIX
AC_DEFUN([LSH_RPATH_FIX],
[if test $cross_compiling = no -a "x$RPATHFLAG" != x ; then
  ac_success=no
  AC_TRY_RUN([int main(int argc, char **argv) { return 0; }],
    ac_success=yes, ac_success=no, :)
  
  if test $ac_success = no ; then
    AC_MSG_CHECKING([Running simple test program failed. Trying -R flags])
dnl echo RPATH_CANDIDATE_DIRS = $RPATH_CANDIDATE_DIRS
    ac_remaining_dirs=''
    ac_rpath_save_LDFLAGS="$LDFLAGS"
    for d in $RPATH_CANDIDATE_DIRS ; do
      if test $ac_success = yes ; then
  	ac_remaining_dirs="$ac_remaining_dirs $d"
      else
  	LDFLAGS="$RPATHFLAG$d $LDFLAGS"
dnl echo LDFLAGS = $LDFLAGS
  	AC_TRY_RUN([int main(int argc, char **argv) { return 0; }],
  	  [ac_success=yes
  	  ac_rpath_save_LDFLAGS="$LDFLAGS"
  	  AC_MSG_RESULT([adding $RPATHFLAG$d])
  	  ],
  	  [ac_remaining_dirs="$ac_remaining_dirs $d"], :)
  	LDFLAGS="$ac_rpath_save_LDFLAGS"
      fi
    done
    RPATH_CANDIDATE_DIRS=$ac_remaining_dirs
  fi
  if test $ac_success = no ; then
    AC_MSG_RESULT(failed)
  fi
fi
])
