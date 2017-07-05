#!/bin/dash
export PHONEBOOK="/home/artem/gitproject/iec-utils/smsget/phonebook"
export MSGDIR="/home/artem/gitproject/work"
export FIFOTRIGGER="${MSGDIR}/fifotr"

./smsget.sh -d -F /dev/ttyUSB0
./smsget.sh -g -F /dev/ttyUSB0
