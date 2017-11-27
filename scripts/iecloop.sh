#!/bin/dash

trap 'kill -- -$$; exit 0' TERM

export IECDIR="/opt/iecd"
export MSGDIR="${IECDIR}/work"
export IECDB="${IECDIR}/iecdb"
export LOGDIR="${IECDIR}/log"

export GSMDEV="/dev/ttyUSB0"

export FIFOTRIGGER="${MSGDIR}/fifotr"
export BROKERSEND="${IECDIR}/broker.sh"

export IECSOCK="${MSGDIR}/iecsock"
export LOGFILE="${IECDIR}/iecd.log"
export PIDFILE="${IECDIR}/iecd.pid"
export PHONEBOOK="${IECDIR}/phonebook"

#export IECSERVER="89.239.185.109"
#export IECPORT="48321"
export IECSERVER="10.90.90.40"
export IECPORT="2404"

cd $IECDIR
./iecproxy -u $IECSOCK -s $IECSERVER -p $IECPORT -l ./ieclink -g ./ginterrog.sh 2>${LOGDIR}/iecproxy.log &
./quemngr -d 3 2>${LOGDIR}/quemngr.log &

exec 2>${LOGDIR}/smsget.log 1>&2

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
