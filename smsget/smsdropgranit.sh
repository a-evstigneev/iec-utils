#!/bin/dash

#. /opt/iecd/service-func
. /$PWD/service-func 

smsparse()
{
	local file comaddr
	awk -v file=$1 -v comaddr=$2 '
		BEGIN {
			FS=","; FILE = file; COMADDR = comaddr; 
			TYPE["M_SP_NA_1"] = 1
			TYPE["M_ME_NC_1"] = 13
		}
		
		{ 
			for (i = 1; i <= NF; ++i) {
				if ($i ~ /norma/)
					$i = 0
				else if ($i ~ /Neispravnost/ || /Pogar/ || /Trevoga/)
					$i = 1
				else if ($i ~ /Net/)
					$i = 1
				else if ($i ~ /t=/)
					$i = substr($i, index($i, "=") + 1)
			}
			
			for (i = 1; i < NF; ++i )
				printf("%s %s %s %s\n", COMADDR, i, TYPE["M_SP_NA_1"], $i) >> FILE
			printf("%s %s %s %s\n", COMADDR, 1, TYPE["M_ME_NC_1"], $NF) >> FILE
			close(file)
		}
	' 
}

comaddr="0"
if [ ! -z $1 ]; then
	comaddr=$1
fi

read sms
if echo "$sms" | grep -q -v -e '[,:]'; then
	exit 2
fi
if echo $sms | grep -q -e 'Vzayty na ohrany' -e 'Snayty s ohrany'; then
	echo "Subject: iec104\r\n\r\nControlled station  = ${comaddr}, ${sms}" | msmtp --debug -a komisyk@gmail.com -t -i komisyk@mail.ru >/dev/null 2>&1
	exit 2
fi

sms=$(echo $sms | sed 's/ gradus C//')

filename=$(mktemp -u -p "${MSGDIR}/drop" "${comaddr}_XXXXXX")
if printf '%s\n' "$sms" | smsparse "$filename" "$comaddr"; then
	if sync $filename; then
		mv "$filename" "${MSGDIR}/drop/`stat -c %i $filename`"
		printf "1" > $FIFOTRIGGER
	else
		exit 1
	fi
fi
