#!/bin/bash
#
# Startup script for the opinfo-client

# Source function library.
. /etc/rc.d/init.d/functions

# This will prevent initlog from swallowing up a pass-phrase prompt.
INITLOG_ARGS=""

# Path to the httpd binary.
opinfo=/usr/local/sbin/opinfo-client
prog=opinfo-client
RETVAL=0

start() {
	echo -n $"Starting $prog: "
	daemon $opinfo
	RETVAL=$?
	echo
	[ $RETVAL = 0 ] && touch /var/lock/subsys/opinfo-client
	return $RETVAL
}
stop() {
	echo -n $"Stopping $prog: "
	killproc $opinfo
	RETVAL=$?
	echo
	[ $RETVAL = 0 ] && rm -f /var/lock/subsys/opinfo-client /var/run/opinfo-client.pid
}

# See how we were called.
case "$1" in
  start)
	start
	;;
  stop)
	stop
	;;
  restart)
	stop
	start
	;;
  *)
	echo $"Usage: $prog {start|stop|restart}"
	exit 1
esac

exit $RETVAL
