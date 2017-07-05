#!/bin/bash
mkfifo /tmp/fifo1
mkfifo /tmp/fifo2
./client 10.73.134.3 >/tmp/fifo2 </tmp/fifo1 &
exec 3</tmp/fifo2 4>/tmp/fifo1 
rm /tmp/fifo1 /tmp/fifo2

read init <&3
echo $init

read compoll <&3
echo $compoll

if [ "$compoll" == "64010600FFFF00000014" ]
then
	# C_IC_NA_1 (100), COT=7 - подтверждение активации общего опроса	
	printf "%s\n" "64010700FFFF00000014" >&4 
	
	# M_SP_NA_1 (1), SQ=0, COT=20 - общий опрос 	
#	printf "%s\n" "01011400030001000000" >&4 
#	printf "%s\n" "01011400030002000000" >&4 
#	printf "%s\n" "01011400030003000000" >&4 
#	printf "%s\n" "01011400030004000000" >&4 

	# M_SP_NA_1 (1), SQ=1, COT=20	
	printf "%s\n" "01841400030001000001010101" >&4 
	
	# M_ME_NC_1 (13), SQ=0, COT=20
#	printf "%s\n" "0D011400030001000029EA083F00" >&4 
#	printf "%s\n" "0D0114000300020000F6B6553F00" >&4 
#	printf "%s\n" "0D0114000300030000C38322BF00" >&4 
#	printf "%s\n" "0D01140003000400000A0F0ABE00" >&4 
	
	# M_ME_NC_1 (13), SQ=1, COT=20
	printf "%s\n" "0D84140003000100000A0F0ABE00C38322BF0029EA083F00F6B6553F00" >&4 
	
	# C_IC_NA_1 COT=10 - завершение активации общего опроса
	printf "%s\n" "64010A00FFFF00000014" >&4 
fi
exec 4>&-

wait
