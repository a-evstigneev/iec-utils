#!/bin/dash
export GSMDEV="/dev/ttyUSB0"

stty raw -echo -F $GSMDEV

#exec 3<$GSMDEV 4>$GSMDEV

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
    
        if echo "$line" | grep -e "OK" -e "ERROR" 1>/dev/null 2>&1; then
            break
        fi
    done
}


#atsend 4 'ATD+89042334439'

#sleep 2

#readreply 3 

printf '%s\r\n' "ATD89042334439;" >/dev/ttyUSB0; sleep 8; printf '%s\r\n' "ATH" >/dev/ttyUSB0
