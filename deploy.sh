
#!/bin/bash
env -i
set -e

BUILD_PATH=$(pwd)

#echo "🛠 Building project ${BUILD_PATH}"
cd ${BUILD_PATH}
source ../../envsetup.sh
cd ${BUILD_PATH}

if [ ! -f "main.cfg" ]; then cp main.cfg-example main.cfg; fi

source main.cfg
# Increment the patch version by 1
((SOURCE_BUILD_PROJECT_BUILD++))

BUILD_VERSION="${SOURCE_BUILD_PROJECT_MAJOR_VERSION}.${SOURCE_BUILD_PROJECT_MINOR_VERSION}"
BUILD_INFO="${SOURCE_BUILD_PROJECT_FILE} v${BUILD_VERSION} b${SOURCE_BUILD_PROJECT_BUILD}"
BUILD_APP_NAME="${SOURCE_BUILD_PROJECT_FILE}"

echo "🛠 Building Project ${BUILD_INFO}"


# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 

echo "🛠 Compiling ${SOURCE_BUILD_PROJECT_FILE} code"
sed -i "s/^TARGET=.*/TARGET=${SOURCE_BUILD_PROJECT_FILE}/" Makefile
make

if [ "$DEVICE_UUID" == "" ]; then 
    # Generate a UUID
    DEVICE_UUID=$(uuidgen -r | awk -F '-' '{print $NF}' | tr '[:lower:]' '[:upper:]')
    sed -i "s/^DEVICE_UUID=.*/DEVICE_UUID=${DEVICE_UUID}/" main.cfg
fi
if [ "$DEVICE_MAC" == "" ]; then 
    # Generate six pairs of random hexadecimal digits
    DEVICE_MAC=$(printf '%02x:%02x:%02x:%02x:%02x:%02x\n' $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)))
    sed -i "s/^DEVICE_MAC=.*/DEVICE_MAC=${DEVICE_MAC}/" main.cfg
fi

CFGF="./deploy/main.cfg"
echo "[config]" > ${CFGF}
# create configuration file
while IFS='=' read -r name value; do
    if [[ ! -z "$name" && ! -z "$value" ]]; then
        echo "$name=$value" >> $CFGF
    fi
done < "main.cfg"

# edit dynamic variables if they reference another variable inside the main.cfg file
sed -i "s|^DEVICE_VERSION=.*|DEVICE_VERSION=${DEVICE_VERSION}|" $CFGF
sed -i "s|^MQTT_MAIN_TOPIC=.*|MQTT_MAIN_TOPIC=\"${MQTT_MAIN_TOPIC}\"|" $CFGF

IFS=',' # Set the Internal Field Separator to a comma
sensorList="${SENSOR_INVENTORY}"
sensorIndex=0

for sensorItem in $SENSOR_INVENTORY; do
    # load the conf file
    tmpfile=$(mktemp)
    tail -n +2 ./sensors/${sensorItem}/${sensorItem}.cfg > "$tmpfile"
    source "$tmpfile"
    #echo "- Loading sensor ./sensors/${sensorItem}/${sensorItem}.cfg configuration"
    rm -f "$tmpfile"

    i=0

    for ((idx=0; idx<${SENSOR_QTY}; idx++)); do


        sensorEnabled="SENSOR_${idx}_ENABLED"
        if [ "${!sensorEnabled}" == "true" ]; then
            sensorCode="SENSOR_${idx}_CODE"
            sensorName="${DEVICE_NAME}-${SENSOR_MODEL}-${!sensorCode} (${DEVICE_BOARD_BRAND})"
            sensorUid="${DEVICE_UUID}-${SENSOR_MODEL}-${!sensorCode}"
            sensorHassMqttTopic="${HASS_MQTT_SENSOR_TOPIC}${DEVICE_NAME}-${DEVICE_UUID}/${!sensorCode}/"
            sensorsQuantity=${sensorIndex}
            sensorDescription="SENSOR_${idx}_DESCRIPTION"
            sensorPollTime="SENSOR_${idx}_POLL_TIME_MSEC"
            sensorDataType="SENSOR_${idx}_DATA_TYPE"
            sensorClass="SENSOR_${idx}_CLASS"
            sensorUnit="SENSOR_${idx}_UNIT"
            
            echo "SENSOR_${sensorIndex}_NAME=\"${sensorName}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_UID=\"${sensorUid}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_INDEX=${sensorIndex}" >> $CFGF
            echo "SENSOR_${sensorIndex}_INDEX_GROUP=${i}" >> $CFGF
            echo "SENSOR_${sensorIndex}_CODE=\"${!sensorCode}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_MODEL=\"${SENSOR_MODEL}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_DESCRIPTION=\"${!sensorDescription}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_HASS_MQTT_PUBLISH_TOPIC=\"${sensorHassMqttTopic}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_POLL_TIME_MSEC=${!sensorPollTime}" >> $CFGF
            echo "SENSOR_${sensorIndex}_VERSION=${SENSOR_VERSION}" >> $CFGF
            echo "SENSOR_${sensorIndex}_SHM_KEY=102${SENSOR_SHM_KEY}" >> $CFGF
            echo "SENSOR_${sensorIndex}_DATA_TYPE=\"${!sensorDataType}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_MANUFACTURER=\"${SENSOR_MANUFACTURER}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_BRAND=\"${SENSOR_BRAND}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_CLASS=\"${!sensorClass}\"" >> $CFGF
            echo "SENSOR_${sensorIndex}_UNIT=\"${!sensorUnit}\"" >> $CFGF

            echo "✔ Sensor ${sensorName} ${!sensorDescription} configured"
        fi
        sensorIndex=$((sensorIndex + 1))
    done


    cd "./sensors/${sensorItem}/"
    #echo "🛠 Compiling ${sensorItem} code"
    make
    echo "✔ ${sensorItem} sensor compiled"
    
    cd ${BUILD_PATH}
    # if build don;t copy to deploy

    cp ./sensors/${sensorItem}/${sensorItem}.bin deploy/
    cp ./sensors/${sensorItem}/${sensorItem}.cfg deploy/

    i=$((i + 1))
done


sed -i "s|^SENSOR_QUANTITY=.*|SENSOR_QUANTITY=${sensorsQuantity}|" $CFGF
sed -i "s|^SENSOR_QUANTITY=.*|SENSOR_QUANTITY=${sensorsQuantity}|" main.cfg


# DISPLAYS
cd "./displays/st7789/"
#echo "🛠 Compiling ${sensorItem} code"
make
echo "✔ st7789 display compiled"

cd ${BUILD_PATH}
cp ./displays/st7789/st7789.bin deploy/
cp ./displays/st7789/st7789.cfg deploy/



cp ${SOURCE_BUILD_PROJECT_FILE} deploy/
rm -f ${SOURCE_BUILD_PROJECT_FILE} 

#mv $CFGF deploy

# just build
if [ "$1" = "build" ]; then exit 0; fi

# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 

if [ "$1" = "dependencies" ]; then
    echo "📦 Packing libraries and dependencies ..."
    libraries="../../libraries"
    if [ ! -d "$libraries" ]; then
            echo "❌ $libraries not found, cannot continue"
            exit 1
    fi

    OLD_IFS="$IFS"
    IFS=','
    # Iterate over the values
    for lib in $SOURCE_BUILD_PROJECT_LIBRARIES; do


        if [ ! -d "${libraries}/${lib}/${DEVICE_CHIP}" ]; then
            echo "❌ ${libraries}/${lib}/${DEVICE_CHIP} not found, please add the libraries before continue"
            exit 1
        fi

        COMPRESS_FILE="deploy/${lib}.tar.gz"
        if [ -f "${COMPRESS_FILE}" ]; then
            rm -f ${COMPRESS_FILE}
        fi
        tar -czf ${COMPRESS_FILE} -C ${libraries}/${lib}/${DEVICE_CHIP}/ .

        echo "✔ Adding $lib libraries"
    done
    IFS="$OLD_IFS"
fi
# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 

# ⬇ Down
sshpass -p "${DEVICE_SSH_PASSWORD}" scp -r deploy ${DEVICE_USER}@${DEVICE_ADDRESS}:/root/
echo "✔ Firmware uploaded to ${DEVICE_ADDRESS}"

#rm -f deploy/*.tar.gz
#rm -f deploy/*.bin
#rm -f deploy/*.cfg

# Check if the first argument is "deploy or debug"
if [ "$1" = "debug" ]; then
    sshpass -p "${DEVICE_SSH_PASSWORD}" ssh ${DEVICE_USER}@${DEVICE_ADDRESS} "sh /root/deploy/install.sh debug"
elif [ "$1" = "dependencies" ]; then
    sshpass -p "${DEVICE_SSH_PASSWORD}" ssh ${DEVICE_USER}@${DEVICE_ADDRESS} "sh /root/deploy/install.sh dependencies"
else
    sshpass -p "${DEVICE_SSH_PASSWORD}" ssh ${DEVICE_USER}@${DEVICE_ADDRESS} "sh /root/deploy/install.sh deploy"
fi


if [ ! $? -eq 0 ]; then
    echo "❌ Failed"
    exit 1
fi

sed -i "s/^SOURCE_BUILD_PROJECT_BUILD=.*/SOURCE_BUILD_PROJECT_BUILD=${SOURCE_BUILD_PROJECT_BUILD}/" main.cfg
echo "✔ Build has finished"