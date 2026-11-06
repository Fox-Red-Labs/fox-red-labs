#!/bin/sh
# On the board. Frees LD8 (GPIO0_123) and SW5 (GPIO0_90) from the kernel drivers that own them,
# so a user-space program can request the lines, or hands them back.
#   gpio_claim.sh free | restore
D=/sys/bus/platform/drivers
case "$1" in
  free)
    echo none > /sys/class/leds/am62-sk:green:heartbeat/trigger 2>/dev/null
    echo leds > $D/leds-gpio/unbind
    echo gpio-keys > $D/gpio-keys/unbind ;;
  restore)
    echo leds > $D/leds-gpio/bind
    echo gpio-keys > $D/gpio-keys/bind ;;
  *) echo "usage: $0 free|restore"; exit 2 ;;
esac
