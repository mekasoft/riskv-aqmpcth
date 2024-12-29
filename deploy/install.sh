#!/bin/sh
env -i
#set -e
echo -e "\033[34m * * * * * * * * * * * Configuring Firmware * * * * * * * * * * * * * * * * *"
# error ❌
DEPLOY_PATH=/root/deploy

cd ${DEPLOY_PATH}
tmpfile=$(mktemp)
tail -n +2 ${DEPLOY_PATH}/main.cfg > "$tmpfile"
source "$tmpfile"
rm -f "$tmpfile"

echo " - - - - Device - - - -"
echo "- Name:  $DEVICE_NAME-$DEVICE_UUID" 
echo "- Desc:  $DEVICE_NAME_DESCRIPTION"
echo "- Model: $DEVICE_MODEL-${DEVICE_VERSION}"
echo "- Board: $DEVICE_BOARD_BRAND"
echo "- Ver:   $SOURCE_BUILD_PROJECT_MAJOR_VERSION.$SOURCE_BUILD_PROJECT_MINOR_VERSION"
echo "- Build: ${SOURCE_BUILD_PROJECT_BUILD}"

echo " - - - - Processes - - - -"
# iterate from the variable SENSOR_INVENTORY separated by comma and echo the value
IFS=',' # Set the Internal Field Separator to a comma
PidList="main,${SENSOR_INVENTORY}"
for sensor in $PidList; do
    # Check if the process exists to stop it 
    pid=$(ps aux | grep "${sensor}.bin" | head -n 1 | awk '{print $1}')
    if [ -n $pid ]; then
        # If the process exists, stop it
        kill $pid
        echo "✔  Process $sensor.bin with PID $pid stopped."
    else
        echo "⚠  Process $sensor.bin is not running."
    fi
done


if [ ! -d tmp ]; then mkdir tmp; fi


if [ "$1" = "dependencies" ]; then
    echo " - - - - Dependencies - - - -"
    # Iterate through each tar.gz file in the directory
    for file in *.tar.gz; do
        # Extract library_name without extension
        library_name=$(basename "$file" .tar.gz)
        # Create a directory for extraction
        tar -xzf "$file" -C "tmp" && rm -f $file
        # change owner to root
        chown 0:0 -R tmp
        # copy files to destination, allfiles should have the real path structure
        cp -Rnp tmp/* / && rm -Rf tmp/*
        echo "✔  ${library_name} Installed"

    done
fi

echo " - - - - Application - - - -"

#cp -f main.bin /root/ && rm -f main.bin
#cp -f .env /root/ && rm -f .env
#cp -f config.ini /root/ && rm -f config.ini
cp -f *.bin /root/ && rm -f *.bin
cp -f *.cfg /root/ && rm -f *.cfg

echo "#!/bin/sh" > auto.sh
echo "# auto generetad don't edit here " >> auto.sh
for sensor in $PidList; do
    echo "/root/${sensor}.bin &" >> auto.sh
done
IFS=' ' # Reset the Internal Field Separator to the default value

chown 0:0 auto.sh && rm /mnt/system/auto.sh && mv auto.sh /mnt/system/ && sync
echo "✔  Main application configuration"

echo " - - - - Modules - - - -"
if [ $MQTT_ENABLED=="True" ]; then
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Name" -m "$DEVICE_NAME-$DEVICE_UUID"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Description" -m "${DEVICE_NAME_DESCRIPTION}"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Name" -m "$DEVICE_UUID"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Build" -m "${SOURCE_BUILD_PROJECT_BUILD}"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Version" -m "$SOURCE_BUILD_PROJECT_MAJOR_VERSION.$SOURCE_BUILD_PROJECT_MINOR_VERSION"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Model" -m "${DEVICE_MODEL}"
    mosquitto_pub -h ${MQTT_HOST} -u ${MQTT_USER} -P ${MQTT_PASS} -p ${MQTT_PORT} -t "${MQTT_MAIN_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/Hardware" -m "${DEVICE_VERSION}"
    echo "✔  MQTT Mosquitto Enabled"
 
fi

echo " - - - - Final Tasks - - - -"
rm -fR tmp
echo "✔  Workspace Cleaned"
# first installation tasks
if [ -f /mnt/system/blink.sh ]; then
    mv /mnt/system/blink.sh /mnt/system/blink.sh_backup && sync
    echo "✔  Blink.sh Renamed"
fi

# if ip address is not equal to $DEVICE_ADDRESS set it 
if [ "$(hostname -I)" != "$DEVICE_ADDRESS" ]; then
    #route add default gw $DEVICE_ADDRESS_GW
    #ifconfig eth0 $DEVICE_ADDRESS netmask $DEVICE_ADDRESS_NM
    echo "✔  IP Address $DEVICE_ADDRESS Set"

fi

echo -e " * * * * * * * * * * * Configuring Firmware  * * * * * * * * * * * * * * * *\033[0m"
if [ "$1" = "deploy" ]; then
    echo "Rebooting..."
    reboot
elif [ "$1" = "debug" ]; then

    echo -e "\033[32m * * * * * * * * * * * Debug Session * * * * * * * * * * * * * * * * *"

    IFS=',' # Set the Internal Field Separator to a comma
    for test_app in $SENSOR_INVENTORY; do
        /root/${test_app}.bin &
    done
    /root/main.bin
    IFS=' ' # Reset the Internal Field Separator to the default value
    echo -e "\033[0m"

fi
