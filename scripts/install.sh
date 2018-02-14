#!/bin/bash

UPDATE_AVR=true
UPDATE_PY=true

while [ $# -gt 0 ]; do
    case "$1" in
        --no-avr) UPDATE_AVR=false ;;
        --no-py) UPDATE_PY=false ;;
    esac
    shift 1
done


if $UPDATE_PY; then
    if [ -e /var/run/bbctrl.pid ]; then
        service bbctrl stop
    fi
fi

if $UPDATE_AVR; then
    ./scripts/avr109-flash.py src/avr/bbctrl-avr-firmware.hex
fi

# Increase USB current
grep max_usb_current /boot/config.txt >/dev/null
if [ $? -ne 0 ]; then
    mount -o remount,rw /boot &&
    echo max_usb_current=1 >> /boot/config.txt
    mount -o remount,ro /boot
fi

if $UPDATE_PY; then
    rm -rf /usr/local/lib/python*/dist-packages/bbctrl-*
    ./setup.py install --force
    service bbctrl start
fi

sync
