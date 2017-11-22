#!/bin/dash

IECDIR="/opt/iecd_with_proxy"
IECDB="$IECDIR/iecdb"

if ! [ -z "$(ls $IECDB)" ]; then 
	find $IECDB -type f -printf "%P\n" | \
	while IFS="/" read comaddr type objaddr;do 
		echo $comaddr $objaddr $type $(cat $IECDB/$comaddr/$type/$objaddr 2>/dev/null) 
	done | $IECDIR/asduconv.sh -c 20
else
	exit 1
fi


#find $IECDB -type f -printf "%P\n" | while IFS="/" read comaddr type objaddr; do echo $comaddr $objaddr $type $(cat $IECDB/$comaddr/$type/$objaddr 2>/dev/null); done # | \
#$IECDIR/asduconv.sh -c 20
