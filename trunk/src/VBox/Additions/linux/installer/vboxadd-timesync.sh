#!/bin/bash
#
#  InnoTek VirtualBox
#
#  Linux Additions timesync daemon init script
#
#  Copyright (C) 2006 InnoTek Systemberatung GmbH
#
#  This file is part of VirtualBox Open Source Edition (OSE), as
#  available from http://www.virtualbox.org. This file is free software;
#  you can redistribute it and/or modify it under the terms of the GNU
#  General Public License as published by the Free Software Foundation,
#  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
#  distribution. VirtualBox OSE is distributed in the hope that it will
#  be useful, but WITHOUT ANY WARRANTY of any kind.
#
#  If you received this file as part of a commercial VirtualBox
#  distribution, then only the terms of your commercial VirtualBox
#  license agreement apply instead of the previous paragraph.
#

# chkconfig: 35 35 56
# description: VirtualBox Additions timesync
#
### BEGIN INIT INFO
# Provides:       vboxadd-timesync
# Required-Start: vboxadd
# Required-Stop:  vboxadd
# Default-Start:  3 5
# Default-Stop:
# Description:    VirtualBox Additions timesync
### END INIT INFO

# We still have some dependency problems to solve
#if [ ! "`uname -r | grep '2.4'`" = "" ]; then
#    echo The VirtualBox time synchronization module currently does not work on 2.4 series Linux kernels
#    exit 0
#fi

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    PIDFILE="/var/lock/subsys/vboxadd-timesync"
elif [ -f /etc/SuSE-release ]; then
    system=suse
    PIDFILE="/var/lock/subsys/vboxadd-timesync"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxadd-timesync"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    PIDFILE="/var/run/vboxadd-timesync"
else
    echo "$0: Unknown system" 1>&2
fi

if [ $system = redhat ]; then
    . /etc/init.d/functions
    fail_msg() {
        echo_failure
        echo
    }

    succ_msg() {
        echo_success
        echo
    }
fi

if [ $system = suse ]; then
    . /etc/rc.status
    daemon() {
        startproc ${1+"$@"}
    }

    fail_msg() {
        rc_failed 1
        rc_status -v
    }

    succ_msg() {
        rc_reset
        rc_status -v
    }
fi

if [ $system = debian ]; then
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }

    killproc() {
        start-stop-daemon --stop --exec $@
    }

    fail_msg() {
        echo "...fail!"
    }

    succ_msg() {
        echo "...done."
    }
fi

if [ $system = gentoo ]; then
    . /sbin/functions.sh
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }

    killproc() {
        start-stop-daemon --stop --exec $@
    }

    fail_msg() {
        echo "...fail!"
    }

    succ_msg() {
        echo "...done."
    }

    if [ "`which $0`" = "/sbin/rc" ]; then
        shift
    fi
fi

binary=/usr/sbin/vboxadd-timesync

test -x $binary || {
    echo "Cannot run $binary"
    exit 1
}

vboxaddrunning() {
    lsmod | grep -q vboxadd[^_-]
}

start() {
    if ! test -f $PIDFILE; then
        echo -n "Starting vboxadd-timesync ";
        vboxaddrunning || {
            echo "VirtualBox Additions module not loaded!"
            exit 1
        }
        daemon $binary --daemonize
        RETVAL=$?
        test $RETVAL -eq 0 && touch $PIDFILE
        succ_msg
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        echo -n "Stopping vboxadd-timesync";
        vboxaddrunning || {
            echo "VirtualBox Additions module not loaded!"
            exit 1
        }
        killproc $binary
        RETVAL=$?
        test $RETVAL -eq 0 && rm -f $PIDFILE
        succ_msg
    fi
    return $RETVAL
}

restart() {
    stop && start
}

dmnstatus() {
    status vboxadd-timesync
}

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
restart)
    restart
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit $RETVAL
