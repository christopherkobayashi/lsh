# Helper functions for the test scripts.

if [ -z "$srcdir" ] ; then
  srcdir=`pwd`
fi

if [ -z "$SERVERFLAGS" ] ; then
    SERVERFLAGS='-q --enable-core'
fi

if [ -z "$CLIENTFLAGS" ] ; then
    CLIENTFLAGS=-q
fi

if [ -z "$HOSTKEY" ] ; then
    HOSTKEY=$srcdir/key-1.private
fi

if [ -z "$PIDFILE" ] ; then
    PIDFILE=`pwd`/lshd.$$.pid
fi

if [ -z "$INTERFACE" ] ; then
    INTERFACE=127.0.0.1
fi

# if [ -z "$USERKEY" ] ; then
#     USERKEY=$srcdir/key-1.private
# fi

# Any error count as failure.
set -e

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
    
    # echo ../lshd -h $HOSTKEY --interface=$INTERFACE \
    #	-p $PORT $LSHD_FLAGS --pid-file $PIDFILE --daemon "$@"
    
    ../lshd -h $HOSTKEY --interface=$INTERFACE \
	-p $PORT $LSHD_FLAGS --pid-file $PIDFILE --daemon "$@"

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
    cmd=$1
    shift
    echo $cmd | ../lsh $CLIENTFLAGS -nt --sloppy-host-authentication \
	--capture-to /dev/null -z -p $PORT "$@" localhost

}

exec_lsh () {
    ../lsh $CLIENTFLAGS -nt --sloppy-host-authentication \
	--capture-to /dev/null -z -p $PORT localhost "$@"
}

spawn_lsh () {
    # echo spawn_lsh "$@"
    ../lsh $CLIENTFLAGS -nt --sloppy-host-authentication \
	--capture-to /dev/null -z -p $PORT "$@" -N localhost &
    at_exit "kill $!"
}

spawn_lshg () {
    # echo spawn_lshg "$@"
    ../lshg $CLIENTFLAGS -p $PORT "$@" -N localhost &
    at_exit "kill $!"
}

at_connect () {
    mini-inetd -m $2 localhost:$1 -- /bin/sh sh -c "$3" &
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
