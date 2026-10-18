#!/system/bin/sh

# WIFI_FIRMWARE_LOADER del Y210. wifi.c (libhardware_legacy) ya hizo insmod de
# ar6000.ko con WIFI_DRIVER_MODULE_ARG; esto es lo que hacia wlan_tool
# loaddriver despues del insmod: esperar a que el firmware responda, levantar
# la interfaz y avisar a wifi.c. Sin esto ath_supplicant arranca antes de
# que exista eth0.

IFNAME=`getprop wifi.interface eth0`

for i in 1 2 3 4 5 6 7 8 9 10; do
    if wmiconfig -i $IFNAME --getRTS > /dev/null 2>&1; then
        ifconfig $IFNAME up
        log -p i -t wlan_loader "ar6000 listo en $IFNAME"
        setprop wlan.driver.status ok
        exit 0
    fi
    sleep 1
done

log -p e -t wlan_loader "ar6000 no respondio en $IFNAME"
setprop wlan.driver.status failed
