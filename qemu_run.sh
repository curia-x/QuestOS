#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

ROOTFS_PATH=/home/book/qemu_resource/rootfs_aarch64
KERNEL_IMAGE_PATH=/home/book/source_code/linux/arch/arm64/boot/Image
DTB_PATH=/home/book/qemu_resource/cortex-a72_gicv3_smp_4.dtb

if [ "$(id -u)" -ne 0 ]; then
    echo "Re-running script as root..."
    exec sudo "$0" "$@"
fi

APPEND="console=ttyAMA0 root=/dev/nfs rw nfsroot=10.0.2.1:$ROOTFS_PATH,tcp,v3 ip=10.0.2.2:::::eth0:off"
APPEND="$APPEND crashkernel=1024M"
APPEND="$APPEND iommu.passthrough=1"
APPEND="$APPEND nokaslr"
APPEND="$APPEND sysrq_always_enabled=1"

INFO=
DEBUG=
BIOS=quest_os.bin
SECURE=on

usage() {
    echo "Usage: $0 [debug|-d|--debug] [info|--info] [-bios FILE|-bios=FILE] [--secure on|off|--secure=on|off]" >&2
    exit 1
}


while [ $# -gt 0 ]
do
    case "$1" in
        -bios)
            if [ $# -lt 2 ]; then
                echo "error: -bios missing argument" >&2
                exit 1
            fi
            BIOS="$2"
            shift 2
            ;;
        -bios=*)
            BIOS="${1#-bios=}"
            shift
            ;;
        --secure)
            if [ $# -lt 2 ]; then
                echo "error: --secure missing argument" >&2
                exit 1
            fi
            SECURE="$2"
            shift 2
            ;;
        --secure=*)
            SECURE="${1#--secure=}"
            shift
            ;;
        debug|--debug|-d)
            DEBUG=1
            shift
            ;;
        info|--info)
            INFO=1
            shift
            ;;
        -h|--help|help)
            usage
            shift
            ;;
        *)
            shift
            ;;
    esac
done

case "$SECURE" in
    on|off)
        ;;
    *)
        echo "error: --secure can only be on or off" >&2
        exit 1
        ;;
esac

set -- \
    qemu-system-aarch64 \
            -kernel "$KERNEL_IMAGE_PATH" \
            -append "$APPEND" \
            -M "virt,virtualization=false,secure=$SECURE,gic-version=3,its=on" \
            -m size=1024M \
            -cpu cortex-a72,pmu=on \
            -smp 4 \
            -bios "$BIOS" \
            -netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
            -device virtio-net-device,netdev=net0

if [ -n "$DEBUG" ]; then
    set -- "$@" -s -S
fi

if [ -n "$INFO" ]; then
    set -- "$@" -monitor stdio
else
    set -- "$@" -nographic
fi

exec "$@"
