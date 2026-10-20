#!/system/bin/sh

# Huawei's legacy hostapd reads /dev/random for WPA handshakes. On this kernel
# it can expose too little entropy and reject the first 4-way handshake, which
# clients report as an incorrect password.
if [ ! -L /dev/random ]; then
    mv /dev/random /dev/random.real 2>/dev/null
    ln -s /dev/urandom /dev/random
fi

# A control socket left by a previous hostapd (killed, not cleaned up) makes
# netd connect before the new one binds ("Connection refused"); netd then
# retries against this hostapd with the driver already reloaded under it and
# athap0 never comes up. Remove it before starting.
rm -f /data/misc/wifi/hostapd/athap0

exec /system/bin/hostapd /data/misc/wifi/hostapd.conf
