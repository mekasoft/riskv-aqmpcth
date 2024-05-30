#!/bin/sh
env -i
set -e
echo -e "\033[34m * * * * * * * * * * * * * * * * * * * * * * * * * * * *"
echo "⚙ Configuring Firmware"

DEPLOY_PATH=/root/deploy

cd ${DEPLOY_PATH}
source ${DEPLOY_PATH}/.env
echo "Application:"
echo "- Name:  $DEVICE_NAME-$DEVICE_UUID" 
echo "- Desc:  $DEVICE_NAME_DESCRIPTION"
echo "- Model: $DEVICE_MODEL-${DEVICE_VERSION}"
echo "- Board: $DEVICE_BOARD_BRAND"
echo "- Ver:   $SOURCE_BUILD_PROJECT_MAJOR_VERSION.$SOURCE_BUILD_PROJECT_MINOR_VERSION"
echo "- Build: ${SOURCE_BUILD_PROJECT_BUILD}"

echo " - - - - "
# Find PID of process named main.bin
pid=$(ps aux | grep '[m]ain\.bin' | awk '{print $1}')
# Check if the process exists
echo "⚙ Stopping main.bin procces..."
if [ -n "$pid" ]; then
    # If the process exists, stop it
    kill "$pid"
    echo "Process main.bin with PID $pid stopped."
else
    echo "Process main.bin is not running."
fi

# Find PID of process named main.bin
pid=$(ps aux | grep '[p]ms5003\.bin' | awk '{print $1}')
# Check if the process exists
echo "⚙ Stopping pms5003.bin procces..."
if [ -n "$pid" ]; then
    # If the process exists, stop it
    kill "$pid"
    echo "Process pms5003.bin with PID $pid stopped."
else
    echo "Process pms5003.bin is not running."
fi

# Find PID of process named main.bin
pid=$(ps aux | grep '[d]ht22\.bin' | awk '{print $1}')
# Check if the process exists
echo "⚙ Stopping dht22.bin procces..."
if [ -n "$pid" ]; then
    # If the process exists, stop it
    kill "$pid"
    echo "Process dht22.binwith PID $pid stopped."
else
    echo "Process dht22.bin is not running."
fi

# Find PID of process named main.bin
pid=$(ps aux | grep '[m]hz19b\.bin' | awk '{print $1}')
# Check if the process exists
echo "⚙ Stopping mhz19b.bin procces..."
if [ -n "$pid" ]; then
    # If the process exists, stop it
    kill "$pid"
    echo "Process mhz19b.binwith PID $pid stopped."
else
    echo "Process mhz19b.bin is not running."
fi

if [ ! -d tmp ]; then mkdir tmp; fi
echo " - - - - "
if [ ! "$1" = "test" ]; then
    # Iterate through each tar.gz file in the directory
    for file in *.tar.gz; do
        # Extract library_name without extension
        library_name=$(basename "$file" .tar.gz)
        # Create a directory for extraction
        tar -xzf "$file" -C "tmp" && rm -f $file
        echo "⚙ Installing $library_name ..."
        # change owner to root
        chown 0:0 -R tmp
        # copy files to destination, allfiles should have the real path structure
        cp -Rnp tmp/* / && rm -Rf tmp/*
        echo "✔ ${library_name} Done"

    done
fi

echo " - - - - "
echo "⚙ Main application setup"
#cp -f main.bin /root/ && rm -f main.bin
cp -f .env /root/ && rm -f .env
#cp -f config.ini /root/ && rm -f config.ini
cp -f *.bin /root/ && rm -f *.bin
cp -f *.cfg /root/ && rm -f *.cfg
chown 0:0 auto.sh && rm /mnt/system/auto.sh && mv auto.sh /mnt/system/ && sync
echo "✔ Done"

echo " - - - - "
echo "⚙ Modules status"
if [ $MQTT_ENABLED=="True" ]; then
    echo "✔ MQTT Mosquitto Enabled"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Name" -m "$DEVICE_NAME-$DEVICE_UUID"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Description" -m "${DEVICE_NAME_DESCRIPTION}"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Name" -m "$DEVICE_UUID"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Build" -m "${SOURCE_BUILD_PROJECT_BUILD}"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Version" -m "$SOURCE_BUILD_PROJECT_MAJOR_VERSION.$SOURCE_BUILD_PROJECT_MINOR_VERSION"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Model" -m "${DEVICE_MODEL}"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Hardware" -m "${DEVICE_VERSION}"
 
fi

echo " - - - - "
echo "⚙ Cleaning"
rm -fR tmp

echo "✔ Done"
echo -e " * * * * * * * * * * * * * * * * * * * * * * * * * * * *\033[0m"
if [ "$1" = "deploy" ]; then
    echo "Rebooting..."
    reboot
elif [ "$1" = "test" ]; then
    #execute the program
    echo "Running sensor program /root/dht22.bin"
    /root/dht22.bin &
    echo "Running sensor program /root/pms5003.bin"
    /root/pms5003.bin &
    echo "Running sensor program /root/mhz19b.bin"
    /root/mhz19b.bin &
    echo "Running main program /root/main.bin"
    /root/main.bin
fi
