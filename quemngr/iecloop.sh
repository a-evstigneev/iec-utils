#!/bin/dash

export MSGDIR="/home/artem/google-drive/ieclink/qmngr/work"
export FIFOTRIGGER="${MSGDIR}/fifotr"
export FIFOLOG="${MSGDIR}/fifolog"
export BROKERSEND="/home/artem/google-drive/ieclink/qmngr/broker.sh"

#mkfifo $FIFOLOG 2>/dev/null

#./syslogclient -n 10.90.90.15 -p 514 &
./quemngr -d 3 -l ./log.txt
