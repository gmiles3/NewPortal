#!/bin/bash

set -x

nodeno=$1
iface=$2
nodecount=3
is_kickstarter_or_unidir_txmitter="$3" # blank for false; nonblank for true
is_unidir="$4"                         # ditto
channelno=11

# we assume nodecount < 11
testdata=DEADBEEF
if [ "$nodeno" -eq 0 ]; then predno=$((nodecount-1)); else predno=$((nodeno-1)); fi

packetsize=$(($(echo "$nodeno:$(date +%s%2N):$testdata" | wc -c)+4))

process () {
	echo "$1-$(date +%s%2N)" | bc -lq
}

FLAGS="-r 0 -b 1 -f $packetsize"

if rfkill; then rfkill unblock all; fi
ip link set dev "$iface" up

#iw dev "$iface" interface add "$moniface" type monitor
#if [ "$?" != 233 ] && [ "$?" != "0" ]; then exit $?; fi
#ip link set "$moniface" up
#iw dev "$moniface" set channel $channelno

setup () {
	ip link set dev "$iface" down
	iw dev "$iface" set monitor otherbss
	ip link set dev "$iface" up
	iw dev "$iface" set channel $channelno
}

if [ -z "$is_unidir" ]; then
	setup
fi

if [ -n "$is_kickstarter_or_unidir_txmitter" ]; then
	while echo "$nodeno:$(date +%s%2N):$testdata"; do
		if [ -z "$is_unidir" ]; then
			break
		else
			sleep 0.25
		fi
		done | stdbuf -i0 -o0 ./tx $FLAGS -m 1 -p "$nodeno" "$iface"
	exit 1
fi

if [ -n "$is_unidir" ]; then
	setup
fi

stdbuf -o0 ./rx $FLAGS -p "$predno" "$iface" |
	if [ -n "$is_unidir" ]; then
		cat
	else
		while read -r; do
			case $REPLY in
				$predno:*)
					transtimestamp="$(echo "$REPLY" | cut -d: -f2)"
					process "$transtimestamp" 1>&2
					data="$(echo "$REPLY" | cut -d: -f3)"
					if [ "$data" != "$testdata" ]; then echo "$data" 1>&2; fi
					echo "$nodeno:$(date +%s%2N):$testdata"
					;;
				*)
					echo "erroneous data input: $REPLY" 1>&2
					;;
			esac
		done | stdbuf -i0 -o0 ./tx $FLAGS -m 1 -p "$nodeno" "$iface"
	fi

#iw dev "$moniface" del

