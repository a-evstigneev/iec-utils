#!/bin/dash

iecquedir="/var/iecque"
export SMSDIR="${iecquedir}/work"
export RELEASEDIR="${iecquedir}/release"
export LOGFILE="${iecquedir}/iecd.log"
export PIDFILE="${iecquedir}/iecd.pid"
export PHONEBOOK="/opt/iecd/phonebook"

. /opt/iecd/service-func

gsmdev="/dev/ttyUSB0"
iecserver="10.90.90.40"

smssend() 
{
if [ "$(ls -A ${SMSDIR})" ]; then 
	logmsgtime "Catalog $SMSDIR is not empty"
	if asdusend.sh -l "iecclient $iecserver"; then
		logmsgtime "ASDU sent successfully to server $iecserver"
		mv ${SMSDIR}/* $RELEASEDIR
	fi
fi
}

rm $LOGFILE

smssend >> $LOGFILE

logmsgtime "Catalog $SMSDIR is empty" >> $LOGFILE

if smsget.sh -c -F $gsmdev; then
	{
	stty raw -echo -F $gsmdev
	smsget.sh -d -F $gsmdev

	while true; do
		if smsget.sh -g -F $gsmdev; then
			smssend
		fi
		sleep 20 
	done
	} >> $LOGFILE
else
	exit 1
fi
