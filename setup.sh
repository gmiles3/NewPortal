#!/bin/bash

set -x

iface=$1
channelno=$2

if [ -n "$(iw dev $iface info | grep monitor)" ]; then exit; fi

if rfkill; then rfkill unblock all; fi
ip link set dev "$iface" up

#iw dev "$iface" interface add "$moniface" type monitor
#if [ "$?" != 233 ] && [ "$?" != "0" ]; then exit $?; fi
#ip link set "$moniface" up
#iw dev "$moniface" set channel $channelno

ip link set dev "$iface" down
iw dev "$iface" set monitor otherbss
ip link set dev "$iface" up
iw dev "$iface" set channel $channelno

#iw dev "$moniface" del

