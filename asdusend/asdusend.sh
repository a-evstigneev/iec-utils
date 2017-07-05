#!/bin/dash

. /home/artem/gitproject/iec-utils/smsget/service-func

# M_EI_NA_1 (COT=4)
end_init="46010400020000000000"
# C_IC_NA_1 (COT=6)
act_poll="64010600FFFF00000014"
# C_IC_NA_1 (COT=7)
con_act_poll="64010700FFFF00000014"
# C_IC_NA_1 (COT=10)
end_act_poll="64010A00FFFF00000014" 

usage()
{
	echo "Usage: $0 [-l Optinal_line ieclink] ASDU_FILE"
}

while getopts "l:h" opt; do
	case $opt in
	l) ieclink=$OPTARG ;;
	*) usage; exit ;;
	esac
done

shift $(($OPTIND - 1))

#trap "kill $! 2> /dev/null; exit 0" INT TERM

mkfifo /tmp/fifo1.$$
mkfifo /tmp/fifo2.$$

$ieclink >/tmp/fifo2.$$ </tmp/fifo1.$$ &

exec 3</tmp/fifo2.$$ 4>/tmp/fifo1.$$ 
rm /tmp/fifo1.$$ /tmp/fifo2.$$ || true

while read asdu <&3; do
	if [ "$asdu" = "$end_init" ]; then
		echo "Station Initialization completed"
	elif [ "$asdu" = "$act_poll" ]; then
		printf '%s\n' "$con_act_poll" >&4
		asduconv.sh -c 20 "${SMSDIR}/*" >&4
		printf '%s\n' "$end_act_poll" >&4
		exec 4>&-
	fi
done

wait %1 
