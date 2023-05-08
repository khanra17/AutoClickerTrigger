#!/system/bin/sh

# Wait till device boot process completes
while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 1
done

xiaomi_trigger_repeater_left