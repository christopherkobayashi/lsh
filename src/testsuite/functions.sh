# Helper functions for the test scripts.

# Any error count as failure.
set -e

# echo srcdir = $srcdir

if [ -z "$TEST_HOME" ] ; then
  TEST_HOME="`pwd`/home"
fi

if [ -z "$LSH_YARROW_SEED_FILE" ] ; then
  LSH_YARROW_SEED_FILE="$TEST_HOME/.lsh/yarrow-seed-file"
fi

# For lsh-authorize
if [ -z "SEXP_CONV" ] ; then
  SEXP_CONV="`pwd`/../sexp-conv"
fi

export LSH_YARROW_SEED_FILE SEXP_CONV

## # Make sure we have a randomness genereetor
## LSH_YARROW_SEED_FILE="`pwd`/yarrow-seed-file"
## export LSH_YARROW_SEED_FILE
## 
## if [ -s "$LSH_YARROW_SEED_FILE" ] ; then : ; else
##     echo "Creating seed file $LSH_YARROW_SEED_FILE"
##     ../lsh-make-seed --sloppy -q -o "$LSH_YARROW_SEED_FILE"
## fi

if [ -z "$LSHD_FLAGS" ] ; then
    LSHD_FLAGS='-q --enable-core'
fi

if [ -z "$LSH_FLAGS" ] ; then
    LSH_FLAGS=-q
fi

if [ -z "$LSHG_FLAGS" ] ; then
    LSHG_FLAGS=-q
fi

if [ -z "$HOSTKEY" ] ; then
    HOSTKEY=$srcdir/key-1.private
fi

if [ -z "$PIDFILE" ] ; then
    PIDFILE=`pwd`/lshd.$$.pid
fi

INTERFACE=127.0.0.1

# if [ -z "$USERKEY" ] ; then
#     USERKEY=$srcdir/key-1.private
# fi

PORT=11147
ATEXIT='set +e'

# Starting with EXIT_FAILURE and changing it to EXIT_SUCCESS on
# success is right, as long as each test script only performs one
# test. If there are several tests, it would be better to set it to
# EXIT_SUCCESS and change it as soon as one test fails.

test_result=1

test_fail () {
    test_result=1
}

test_success () {
    test_result=0
}

trap 'eval "$ATEXIT ; exit \$test_result"' 0

at_exit () {
  ATEXIT="$ATEXIT ; $1"
}

spawn_lshd () {

    # local is not available in /bin/sh
    # local delay

    # Note that --daemon not only forks into the background, it also changes
    # the cwd, uses syslog, etc.
    
    HOME="$TEST_HOME" ../lshd -h $HOSTKEY --interface=$INTERFACE \
	-p $PORT $LSHD_FLAGS \
	--pid-file $PIDFILE --daemon --no-syslog "$@"

    # lshd may catch the ordinary TERM signal, leading to timing
    # problems when the next lshd process tries to bind the port.
    # So we kill it harder.

    at_exit 'kill -9 `cat $PIDFILE`; rm -f $PIDFILE'

    # Wait a little for lshd to start
    for delay in 1 1 1 1 1 5 5 5 20 20 60 60; do
	if [ -s $PIDFILE ]; then
	    # And a little more for it to open its port
	    sleep 5
	    return
	fi
	sleep $delay
    done
    
    false
}

run_lsh () {
    cmd="$1"
    shift
    echo "$cmd" | HOME="$TEST_HOME" ../lsh $LSH_FLAGS -nt \
	--sloppy-host-authentication \
	--capture-to /dev/null -z -p $PORT "$@" localhost

}

exec_lsh () {
    HOME="$TEST_HOME" ../lsh $LSH_FLAGS -nt --sloppy-host-authentication \
	--capture-to /dev/null -z -p $PORT localhost "$@"
}

# FIXME: Use -B
spawn_lsh () {
    # echo spawn_lsh "$@"
    HOME="$TEST_HOME" ../lsh $LSH_FLAGS -nt --sloppy-host-authentication \
	--capture-to /dev/null -z -p $PORT "$@" -N localhost &
    at_exit "kill $!"
}

exec_lshg () {
    ../lshg $LSHG_FLAGS -nt -p $PORT localhost "$@"
}

spawn_lshg () {
    # echo spawn_lshg "$@"
    ../lshg $LSHG_FLAGS -p $PORT "$@" -N localhost &
    at_exit "kill $!"
}

at_connect () {
    mini-inetd -m $2 -- localhost:$1 /bin/sh sh -c "$3" &
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
