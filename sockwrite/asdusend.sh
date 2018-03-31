#!/bin/dash

. /opt/iecd/service-func

# M_EI_NA_1 (COT=4)
end_init="46010400020000000000"

# C_IC_NA_1 (COT=6)
act_interrog="64010600FFFF00000014" 

# C_IC_NA_1 (COT=7)
con_act_interrog="64010700FFFF00000014"

# C_IC_NA_1 (COT=10)
end_act_interrog="64010A00FFFF00000014" 

pid=$(echo $$)

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

mkfifo /tmp/fifo1.$$
mkfifo /tmp/fifo2.$$

$ieclink >/tmp/fifo2.$$ </tmp/fifo1.$$ &
exec 3</tmp/fifo2.$$ 4>/tmp/fifo1.$$ 

rm /tmp/fifo1.$$ /tmp/fifo2.$$ || true

while read asdu <&3; do
	if echo "$asdu" | grep -q "460104"; then
		echo "asdusend [$pid]: read asdu M_EI_NA_1 (COT=4) $asdu"	
	elif [ "$asdu" = "$act_interrog" ]; then
		echo "asdusend [$pid]: read asdu C_IC_NA_1 (COT=6) $asdu"
		
		printf '%s\n' "$con_act_interrog" >&4
		echo "asdusend [$pid]: send asdu C_IC_NA_1 (COT=7) $con_act_interrog"
		
		/opt/iecd/asduconv.sh -c 20 "$1" >&4
		echo "asdusend [$pid]: send msg $(basename $1)"
		
		printf '%s\n' "$end_act_interrog" >&4
		echo "asdusend [$pid]: send asdu C_IC_NA_1 (COT=10) $end_act_interrog"
		
		exec 4>&-
	fi
done

wait %1 
