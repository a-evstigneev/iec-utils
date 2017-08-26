#!/bin/dash

trap 'kill -- -$$; exit 0' TERM

iecdir="/opt/iecd"
export MSGDIR="${iecdir}/work"
export LOGFILE="${iecdir}/iecd.log"
export PIDFILE="${iecdir}/iecd.pid"
export PHONEBOOK="${iecdir}/phonebook"
export FIFOTRIGGER="${MSGDIR}/fifotr"
export BROKERSEND="${iecdir}/broker.sh"
export GSMDEV="/dev/ttyUSB0"
export IECSERVER="10.90.90.40"

cd /opt/iecd
cp /dev/null quemngr.log
cp /dev/null smsget.log
cp /dev/null iecclient.log

./quemngr -d 3 -l ./quemngr.log &

exec 2>/opt/iecd/smsget.log 1>&2

if ./smsget.sh -c -F $GSMDEV; then
	stty raw -echo -F $GSMDEV
	./smsget.sh -d -F $GSMDEV
	while true; do
		./smsget.sh -g -F $GSMDEV
		sleep 15 
	done 
else
	exit 1
fi
