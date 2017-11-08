#!/bin/dash

trap 'kill -- -$$; exit 0' TERM

export IECDIR="/opt/iecd_with_proxy"
export MSGDIR="${IECDIR}/work"
export LOGFILE="${IECDIR}/iecd.log"
export PIDFILE="${IECDIR}/iecd.pid"
export PHONEBOOK="${IECDIR}/phonebook"
export FIFOTRIGGER="${MSGDIR}/fifotr"
export BROKERSEND="${IECDIR}/broker.sh"
export GSMDEV="/dev/ttyUSB0"
export IECSERVER="10.90.90.40"
export IECLINK="${IECDIR}/ieclink"
export IECSOCK="${IECDIR}/iecsock"

cd $IECDIR
cp /dev/null quemngr.log
cp /dev/null smsget.log
cp /dev/null iecclient.log

./iecproxy -s $IECSOCK -d $IECSERVER -l $IECLINK 2>./iecproxy.log &

./quemngr -d 3 -l ./quemngr.log &

exec 2>./smsget.log 1>&2

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
