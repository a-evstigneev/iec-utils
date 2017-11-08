#!/bin/dash

. ${IECDIR}/service-func

usage() 
{
	echo "Usage: $0 [-idcg] -F DEVICE"
	echo " "
	echo "  -i  Device information."
	echo "  -d  Device initialization."
	echo "  -c  Device check."
	echo "  -g  Receiving messages from the device."
	echo "  -F  Name of gsm-device (example: /dev/ttyUSB0)"
	echo " "
}

atsend()
{
	local fd cmd
	fd=$1
	cmd=$2
	
	printf '%s\r\n' "$cmd" >&"$fd"
}

readreply()
{
	local fd
	fd=$1

	while read <&"$fd" line; do
		if echo "$line" | grep '^[[:space:]]\{1\}' 1>/dev/null 2>&1; then
			continue
		fi
		
		logmsg "$line"
		if echo "$line" | grep -e "OK" -e "ERROR" 1>/dev/null 2>&1; then
			break
		fi
	done
}

smssep()
{
	local listid smsnum comaddr
	
	listid=""; smsnum=0; comaddr=""
	
	while IFS="," read id stat num sp date; do
		if echo "$id" | grep -q 'OK'; then
			if [ $smsnum -eq 0 ]; then
				logmsgtime "Sms list is empty"
				exit 1
			fi
			break
		fi
		
		read sms
		
		id=$(echo $id | sed 's/+CMGL: //')
		sms=$(echo $sms | sed 's/\r$//')
		num=$(echo $num | sed 's/"//g; s/+//')
		logmsg "Read sms_id = ${id}, number = ${num}, sms = ${sms}"
	
		comaddr=$(grep -q $num $PHONEBOOK && grep $num $PHONEBOOK | awk '{ print $2 }')
		if [ $comaddr ]; then
			senddevice=$(grep $num $PHONEBOOK | awk '{ print $3 }')
			
			case $senddevice in
			granit)
				printf '%s\n' "$sms" | ./smsdrop_granit.sh $comaddr &
			;;
			verset)
				printf '%s\n' "$sms" | ./smsdrop_verset.sh $comaddr &
			;;
			esac
			wait %1
			
			case $? in
			0)
				logmsg "Sms id $id was written synchronously to disk"
				listid="${listid}${id} "
			;;
			1)
				logmsg "Synchronous recording to disk failed"
			;;
			2)
				logmsg "Sms message format is incorrect"
				listid="${listid}${id} "
			;;
			esac
		else
			logmsg "Phone number $num is not valid"
			listid="${listid}${id} "
		fi
		
		smsnum=$(($smsnum + 1))
	done
	
	for i in $listid; do
		atsend 4 "AT+CMGD=$i"
		logmsg "Delete sms id $i"
		readreply 3
		atsend 4 "AT+CMGD=$i"
	done
}

if [ $# -eq 0 ]; then
	usage
	exit 0
fi

while getopts "cdghiF:" opt; do
	case $opt in
	F) gsmdev=$OPTARG ;;
	i) info=1 ;;
	d) devinit=1 ;;
	g) get=1 ;;
	c) check=1 ;;
	*) usage; exit ;;
	esac
done

shift $(($OPTIND - 1)) 

# Test gsm-equipment
if [ $check ]; then
	if [ -z $gsmdev ]; then
		logmsgtime "Target device $gsmdev is not specified"
		usage
		exit 1
	elif [ ! -c $gsmdev ]; then 
		logmsgtime "Device $gsmdev does not character"
		exit 1
	fi
fi

exec 3<$gsmdev 4>$gsmdev

# Info about gsm-equipment
if [ $info ]; then
	logmsgtime "Information about $gsmdev"
	for cmd in "AT#LSCRIPT" "AT+CREG?" "AT+CPIN?" "AT+CSQ" "AT+CREG?" "AT+COPS?"; do
		atsend 4 $cmd
		sleep 1
		readreply 3
	done
fi

# Gsm device init
if [ $devinit ]; then
	logmsgtime "Gsm device $gsmdev initialization"
	for cmd in 'AT+CSCS="GSM"' "AT+CSMS=0" 'AT+CMGF=1' 'AT+CPMS="SM","SM","SM"' 'AT+CNMI=2,0,0,0,0'; do
		logmsg $cmd
		atsend 4 $cmd
		sleep 1
		readreply 3
	done
fi

# Get sms from gsm-equipment
if [ $get ]; then
	logmsgtime "Get sms list from $gsmdev"
	atsend 4 'AT+CMGL="ALL"'
	sleep 2
	readreply 3 | smssep
fi
