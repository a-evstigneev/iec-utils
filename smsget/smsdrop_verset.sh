#!/bin/dash

smsparse()
{
	local file comaddr
	awk -v file=$1 -v comaddr=$2 '
		BEGIN {
			FS=" "; FILE = file; COMADDR = comaddr; 
			TYPE["M_SP_NA_1"] = 1
		}
		
		{ 
			value = 0
			objaddr = 0
			
			if ( $0 ~ "Взята на охрану зона" )
				objaddr = $8
			else if ( $0 ~ "Восстановление технолог. зоны" )
				objaddr = $7  
			else if ( $0 ~ "Нарушение технологической зоны" || $0 ~ "Неисправность в зоне" ) {
				objaddr = $7; value = 1
			}
			
			# TC 10
			if ( $0 ~ "Прибор закрыт" ) 
				objaddr = 10
			else if ( $0 ~ "Прибор открыт" ) {
				objaddr = 10; value = 1
			}
			
			# TC 11
			if ( $0 ~ "Сеть двести двадцать вольт включена" )
				objaddr = 11
			else if ( $0 ~ "Сеть двести двадцать вольт выключена" ) {
				objaddr = 11; value = 1  
			}

			# TC 12
			if ( $0 ~ "Аккумулятор в норме" )
				objaddr = 12
			else if ( $0 ~ "Аккумулятор отсутствует" || $0 ~ "Аккумулятор разряжен" ) {
				objaddr = 12; value = 1  
			}
			
			printf("%s %s %s %s\n", COMADDR, objaddr, TYPE["M_SP_NA_1"], value) >> FILE
			close(file)
		}
	' 
}

comaddr="0"
if [ ! -z $1 ]; then
	comaddr=$1
fi

read sms
smsconv=$(/bin/echo -n -e `/bin/echo $sms | sed -r 's/.{2}/\\\\x&/g'` | iconv -f UCS-2BE)
filename=$(mktemp -u -p "${MSGDIR}/drop" "${comaddr}_XXXXXX")

if printf '%s\n' "$smsconv" | smsparse "$filename" "$comaddr"; then
	true
	if sync $filename; then
		mv "$filename" "${MSGDIR}/drop/`stat -c %i $filename`"
		printf "1" > $FIFOTRIGGER
	else
		exit 1
	fi
fi
