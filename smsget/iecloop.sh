#!/bin/dash

iecdir="/opt/iecd"
export MSGDIR="${iecdir}/work"
export LOGFILE="${iecdir}/iecd.log"
export PIDFILE="${iecdir}/iecd.pid"
export PHONEBOOK="${iecdir}/phonebook"
export FIFOTRIGGER="${MSGDIR}/fifotr"
export BROKERSEND="${iecdir}/broker.sh"
export GSMDEV="/dev/ttyUSB0"
export IECSERVER="10.90.90.40"

. /opt/iecd/service-func

./quemngr -d 3 -l ./quemngrlog.txt &

if ./smsget.sh -c -F $GSMDEV; then
	stty raw -echo -F $GSMDEV
	./smsget.sh -d -F $GSMDEV
	while true; do
		./smsget.sh -g -F $GSMDEV
		sleep 10 
	done
else
	exit 1
fi
