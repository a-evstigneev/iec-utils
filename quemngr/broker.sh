#!/bin/dash

tmpdir=$(mktemp -d /tmp/XXXXX.broker)
test -n "$MSGDIR" && actdir=${MSGDIR}/act

sendfile()
{
	/opt/iecd/asdusend.sh -l "/opt/iecd/iecclient $IECSERVER" $1
}

jobslist=${tmpdir}/jobslist
sendfiles=${tmpdir}/sendfiles

success="Transfer was successful"
fail="Transfer failed"
conf="Ok"

cleanup() 
{
	rm -rf ${tmpdir}
}

sigchld()
{
	local pid state code name
	
	trap '' CHLD
	jobs -l >${jobslist}
	while read pid state; do
		if test -z "$state" || printf '%s' "$state" | grep -q '^Running\|^Stopped\|^Suspended'; then
			continue
		else
			case "$state" in
			Done*)
				code=$(printf '%s' "$state" | sed -n 's|^Done(\([0-9]\+\))$|\1|p')
				test -z $code && code=0
				name=$(sed -n "s/$pid //p" $sendfiles)
				test -n $name && sed -i "/$pid/d" $sendfiles
				case "$code" in
				0) 
					printf '%s\n' "$name $code $success" 
					;;
				*) 
					printf '%s\n' "$name $code $fail" 
					;;
				esac
				;;
			*)
				true
				;;
			esac
		fi
	done <<-EOF
		$(sed 's/^.* \([0-9].*\)/\1/' <${jobslist})	
	EOF
	trap 'sigchld' CHLD
}

trap 'sigchld' CHLD
#trap 'cleanup; exit 1' INT QUIT PIPE TERM 

while true; do 
	if read filename; then 
		printf '%s\n' "$filename 1 $conf"
		sendfile ${actdir}/$filename & 
		printf '%s %s\n' $! $filename >> $sendfiles
	fi
done
