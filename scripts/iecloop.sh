#!/bin/dash

trap 'kill -- -$$; exit 0' TERM

export IECDIR="/opt/iecd"
export MSGDIR="${IECDIR}/work"
export IECDB="${IECDIR}/iecdb"
export LOGDIR="${IECDIR}"

export GSMDEV="/dev/ttyUSB0"

export FIFOTRIGGER="${MSGDIR}/fifotr"
export BROKERSEND="${IECDIR}/broker.sh"

export IECLINK="${IECDIR}/ieclink"
export IECSOCK="${IECDIR}/iecsock"

export LOGFILE="${IECDIR}/iecd.log"
export PIDFILE="${IECDIR}/iecd.pid"

export PHONEBOOK="${IECDIR}/phonebook"

#export IECSERVER="89.239.185.109"
#export IECPORT="48321"
export IECSERVER="10.90.90.40"
export IECPORT="2404"

cd $IECDIR
cp /dev/null quemngr.log
cp /dev/null smsget.log
cp /dev/null iecproxy.log

./iecproxy -u $IECSOCK -s $IECSERVER -p $IECPORT -l ./ieclink -g ./ginterrog.sh 2>${LOGDIR}/iecproxy.log &
#./iecproxy -u $IECSOCK -s $IECSERVER -p $IECPORT -l $IECLINK -g $IECDIR/ginterrog.sh 2>./iecproxy.log &

./quemngr -d 3 2>${LOGDIR}/quemngr.log &
#./quemngr -d 3 -l ./quemngr.log &

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
