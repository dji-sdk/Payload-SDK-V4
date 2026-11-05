#!/usr/bin/env bash
# psdk-net-setup.sh — bring the aircraft-facing network up before the payload dials the broker.
#
# Wire it into the systemd unit as a pre-start hook (see psdk-iot-upgrade.service):
#     ExecStartPre=-/usr/local/bin/psdk-net-setup.sh
# The leading '-' makes a non-zero exit non-fatal to the service start (network bring-up is
# best-effort; the payload itself keeps retrying the broker afterwards).
#
# This is the shell twin of UpgradePlatformLinux_EnsureNetwork() in
# projects/linux/upgrade_platform_linux.c — kept identical on purpose so the two deployment
# styles (systemd ExecStartPre vs. in-process helper) behave the same. Every step is idempotent.
#
# Topology (CUBE/RPi test rig): the aircraft hands out DHCP on the uplink NIC; if a bridge exists
# the uplink IP is moved onto it, then the broker subnet is routed via the uplink's DHCP gateway.
#
# Tunables (env, same names/defaults as the C helper):
#     THING_MODEL_NET_UPLINK        aircraft-facing DHCP NIC        (default usb0)
#     THING_MODEL_NET_BRIDGE        IP-migration target bridge      (default pi4br0; skip if absent)
#     THING_MODEL_NET_BROKER_SUBNET broker subnet to route          (default 192.0.2.0/24)
#     THING_MODEL_NET_GATEWAY       next-hop to reach the broker subnet; overrides auto-detection.
#                                   Auto-detect order: this > uplink's DHCP default route > any
#                                   default route. Set it when the aircraft link installs no default
#                                   route (then the subnet route can't be inferred and is skipped).
#     THING_MODEL_NET_WAIT          seconds to wait for the uplink to appear + get a DHCP lease at
#                                   boot before configuring (default 30). The USB gadget/bridge come
#                                   up after this hook first runs, so a wait is required.
#     THING_MODEL_BROKER            broker host/IP; if already pingable the script does nothing
#                                   (may also be passed as $1)
set -u

UPLINK="${THING_MODEL_NET_UPLINK:-usb0}"
BRIDGE="${THING_MODEL_NET_BRIDGE:-pi4br0}"
SUBNET="${THING_MODEL_NET_BROKER_SUBNET:-192.0.2.0/24}"
GWOVR="${THING_MODEL_NET_GATEWAY:-}"
BROKER="${THING_MODEL_BROKER:-${1:-}}"

log() { echo "[psdk-net-setup] $*"; }

# 0. Short-circuit: broker already reachable -> nothing to do (idempotent re-runs, warm boots).
if [ -n "$BROKER" ] && ping -c 1 -W 1 "$BROKER" >/dev/null 2>&1; then
    log "broker $BROKER already reachable — no network changes needed"
    exit 0
fi

log "configuring network (uplink=$UPLINK bridge=$BRIDGE subnet=$SUBNET broker=${BROKER:-unset})"

# 1. Wait for the UPLINK to get its aircraft DHCP lease — an IPv4 address AND a default gateway.
#    Two things matter here:
#      · At boot the USB gadget (usb0) is created a few seconds after this hook first runs, and its
#        DHCP lease lands later still — so poll, don't read once.
#      · Read ONLY from the uplink. The bridge (pi4br0) permanently carries its OWN local address
#        (e.g. 192.0.2.120/16); keying off "the bridge has an address" fires immediately and
#        grabs that wrong local IP as if it were the aircraft subnet (the exact bug we hit). The
#        aircraft network is the ONE that arrives with a default route on usb0.
awk_gw='/via/{for(i=1;i<=NF;i++) if($i=="via"){print $(i+1); exit}}'
WAIT="${THING_MODEL_NET_WAIT:-45}"
CIDR=""
GW="$GWOVR"
i=0
while :; do
    if ip link show "$UPLINK" >/dev/null 2>&1; then
        CIDR="$(ip -4 -o addr show dev "$UPLINK" 2>/dev/null | awk '{print $4}' | head -1)"
        [ -z "$GW" ] && GW="$(ip -4 -o route show default dev "$UPLINK" 2>/dev/null | awk "$awk_gw")"
        [ -n "$CIDR" ] && [ -n "$GW" ] && break                      # lease + gateway both present
        [ -z "$CIDR" ] && { dhclient -1 "$UPLINK" >/dev/null 2>&1 || true; }   # nudge DHCP
    fi
    i=$((i+1))
    if [ "$i" -ge "$WAIT" ]; then
        log "uplink $UPLINK not fully up after ${WAIT}s (CIDR='${CIDR:-none}' GW='${GW:-none}') — proceeding best-effort"
        break
    fi
    sleep 1
done

if [ -z "$CIDR" ]; then
    log "no DHCP lease on $UPLINK — cannot determine the aircraft subnet (is the aircraft powered/linked?)"
    exit 1
fi

DEV="$UPLINK"
if ip link show "$BRIDGE" >/dev/null 2>&1; then
    DEV="$BRIDGE"
    ip addr del "$CIDR" dev "$UPLINK" 2>/dev/null   # move, don't duplicate
    ip addr replace "$CIDR" dev "$BRIDGE"
    log "moved $CIDR onto bridge $BRIDGE"
fi

if [ -n "$GW" ]; then
    ip route replace "$SUBNET" via "$GW" dev "$DEV" src "${CIDR%%/*}"
    log "routed $SUBNET via $GW dev $DEV (src ${CIDR%%/*})"
elif [ -n "$BROKER" ] && ip route get "$BROKER" >/dev/null 2>&1; then
    # No gateway, but the kernel already has a path to the broker (it sits on the directly-connected
    # subnet) — the on-link route from our address covers it, nothing to add.
    log "no gateway needed — broker $BROKER is directly on-link ($DEV $CIDR)"
else
    log "no gateway to $SUBNET (uplink has no default route). If the broker is routed rather than "\
"on-link, set THING_MODEL_NET_GATEWAY=<next-hop-ip> in the unit; route left unset."
fi

# Final reachability report (non-fatal either way; the payload keeps retrying the broker).
if [ -n "$BROKER" ]; then
    if ping -c 1 -W 1 "$BROKER" >/dev/null 2>&1; then
        log "network up, broker $BROKER reachable"
    else
        log "broker $BROKER still unreachable (payload will keep retrying)"
    fi
fi
exit 0
