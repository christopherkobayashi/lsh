# Helper functions for the test scripts.

# echo srcdir = $srcdir

type_p () {
  ( IFS=':'
    for d in $PATH ; do
      if [ -x "$d/$1" ] ; then
        echo "$d/$1"
	exit 0
      fi
    done
    exit 1
  ) }

: ${TEST_HOME:=`pwd`/home}
: ${LSH_YARROW_SEED_FILE:="$TEST_HOME/.lsh/yarrow-seed-file"}

: ${LFIB_STREAM:="`PATH="../nettle_builddir/tools:$PATH" type_p nettle-lfib-stream`"}
: ${SEXP_CONV:="`PATH="../nettle_builddir/tools:$PATH" type_p sexp-conv`"}

: ${LSH_TRANSPORT:="`cd .. && pwd`/lsh-transport"}
LSH_MAKE_SEED=/bin/false

: ${LSHD_LIBEXEC_DIR="`cd .. && pwd`"}
: ${LSHD_USERAUTH:="`cd .. && pwd`/lshd-userauth"}

: ${LSHD_UTMP:="`pwd`/home/utmpx"}
: ${LSHD_WTMP:="`pwd`/home/wtmpx"}

: ${LSHD_CONFIG_DIR:="$srcdir/config"}

: ${XAUTH:="`PATH="$PATH:/usr/openwin/bin:/usr/X11R6/bin" type_p xauth`"}
: ${XMODMAP:="`PATH="$PATH:/usr/openwin/bin:/usr/X11R6/bin" type_p xmodmap`"}
: ${XVFB:="`PATH="$PATH:/usr/openwin/bin:/usr/X11R6/bin" type_p Xvfb`"}

: ${ENV_PROGRAM:="`type_p env`"}

GETPWNAM_PRELOAD="`pwd`/getpwnam-wrapper.so"

export LSH_YARROW_SEED_FILE SEXP_CONV LSH_TRANSPORT
export LSHD_LIBEXEC_DIR
export LSHD_UTMP LSHD_WTMP LSHD_CONFIG_DIR

: ${LSHD_FLAGS:=''}
: ${LSH_FLAGS:=-q}
: ${LSHG_FLAGS:=-q}
: ${HOSTKEY:="$srcdir/key-1.private"}
: ${PIDFILE:="`pwd`/lshd.$$.pid"}
: ${LSH_PIDFILE:="`pwd`/lsh.$$.pid"}
: ${LSHG_PIDFILE:="`pwd`/lshg.$$.pid"}
: ${INTERFACE:=localhost}

unset DISPLAY

TEST_DISPLAY=:17
XAUTHORITY="$TEST_HOME/xauthority"
export XAUTHORITY

# FIXME: Are these flags obsolete?

# Ignore any options the tester might have put in the environment.

# With bash, unset returns a non-zero exit status for non-existing
# variables. We have to ignore that error.

unset LSHGFLAGS || :
unset LSHFLAGS || :

PORT=11147
ATEXIT='true '

# We start with EXIT_FAILURE, and changing it to EXIT_SUCCESS only if
# test_success is invoked.

test_result=1

werror () {
    echo 1>&2 "$1"
}

test_done () {
    eval "$ATEXIT"
    exit $test_result;
}

test_fail () {
    test_result=1
    test_done
}

test_success () {
    test_result=0
    test_done
}

test_skip () {
    test_result=77
    test_done
}

die () {
    werror "$1"
    test_fail
}

check_x11_support () {
    ../lsh --help | grep 'x11-forward' >/dev/null || test_skip
}

need_xvfb () {
    check_x11_support
    [ -n "$XVFB" ] || test_skip
}

need_tcputils () {
    type tcpconnect >/dev/null 2>&1 || test_skip
}

need_tsocks () {
    type tsocks >/dev/null 2>&1 || test_skip
}

need_getpwnam_wrapper () {
    [ -f "getpwnam-wrapper.so" ] || test_skip
}

at_exit () {
  ATEXIT="$ATEXIT ; $1"
}

spawn_lshd () {

    # local is not available in /bin/sh
    # local delay

    HOME="$TEST_HOME" ../lshd -h $HOSTKEY \
	-p $PORT --interface=$INTERFACE $LSHD_FLAGS \
	--pid-file $PIDFILE --daemonic --no-setsid "$@" || return 1
    
    # lshd should release its port after receiving HUP, but we may get
    # timing problems when the next lshd process tries to bind the
    # port. So we also wait a little.

    at_exit 'kill -HUP `cat $PIDFILE` ; sleep 5'

    # Wait a little for lshd to start
    for delay in 1 1 1 1 1 5 5 5 20 20 60 60; do
	if [ -s $PIDFILE ]; then
	    # And a little more for the pid file to be written properly
	    sleep 1
	    echo lshd pid: `cat $PIDFILE`
	    return 0
	fi
	sleep $delay
    done
    
    return 1
}

# FIXME: Enable -z, when implemented on the server side.
run_lsh () {
    cmd="$1"
    shift
    echo "$cmd" | HOME="$TEST_HOME" ../lsh -nt $LSH_FLAGS \
	--no-use-gateway --sloppy-host-authentication \
	--host-db-update /dev/null -p $PORT "$@" localhost
}

stdin_lsh () {
    HOME="$TEST_HOME" ../lsh -nt $LSH_FLAGS \
	--no-use-gateway --sloppy-host-authentication \
	--host-db-update /dev/null -p $PORT "$@" localhost
}

exec_lsh () {
    HOME="$TEST_HOME" ../lsh $LSH_FLAGS --sloppy-host-authentication \
	--no-use-gateway \
	--host-db-update /dev/null -z -p $PORT localhost "$@"
}

spawn_lsh () {
    # echo spawn_lsh "$@"
    HOME="$TEST_HOME" ../lsh $LSH_FLAGS -nt --sloppy-host-authentication \
	--no-use-gateway \
	--host-db-update /dev/null -z -p $PORT "$@" --write-pid -B localhost > "$LSH_PIDFILE"

    at_exit 'kill `cat $LSH_PIDFILE`'
}

exec_lshg () {
    ../lsh --use-gateway --program-name lshg $LSHG_FLAGS -p $PORT localhost "$@"
}

spawn_lshg () {
    # echo spawn_lshg "$@"
    ../lsh --use-gateway --program-name lshg $LSHG_FLAGS -p $PORT "$@" --write-pid -B localhost > "$LSHG_PIDFILE"
    at_exit 'kill `cat $LSHG_PIDFILE`'
}

spawn_xvfb () {
    rm -f $XAUTHORITY
    # Set up authorization the old-fashioned way; The security
    # extension used by xauth generate is not always supported.
    $XAUTH <<EOF
add $TEST_DISPLAY . `../lsh-keygen -a symmetric --bit-length 128`
EOF
    
    $XVFB -auth $XAUTHORITY -nolisten tcp $TEST_DISPLAY &
    at_exit "kill $!"
    sleep 3
}


# at_connect local-port max-connections shell-command Note: Doesn't
# use -m $2 to set max connections. mini-inetd is always terminated
# by the below kill.
at_connect () {
    # sleep 1 # Allow some time for earlier processes to die
    ./mini-inetd -- localhost:$1 /bin/sh sh -c "$3" &
    at_exit "kill $!"
}

compare_output() {
    if cmp test.out1 test.out2; then
	echo "$1: Ok, files match."
	test_success
    else
	echo "$1: Error, files are different."
	test_fail
    fi
}
