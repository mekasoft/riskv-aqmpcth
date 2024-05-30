
#!/bin/bash
env -i
set -e

BUILD_PATH=$(pwd)

echo "🛠 Preparing project ${BUILD_PATH}"
cd ${BUILD_PATH}
source ../../envsetup.sh
#cd ${BUILD_PATH}
source .env
# Increment the patch version by 1
((SOURCE_BUILD_PROJECT_BUILD++))

BUILD_VERSION="${SOURCE_BUILD_PROJECT_MAJOR_VERSION}.${SOURCE_BUILD_PROJECT_MINOR_VERSION}"
BUILD_INFO="${SOURCE_BUILD_PROJECT_FILE} v${BUILD_VERSION} b${SOURCE_BUILD_PROJECT_BUILD}"
BUILD_APP_NAME="${SOURCE_BUILD_PROJECT_FILE}"

echo "🛠 Building Project ${BUILD_INFO}"

if [ ! -f ".env" ]; then cp .env.copy .env; fi

# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 

echo "🛠 Compiling ${SOURCE_BUILD_PROJECT_FILE} code"
sed -i "s/^TARGET=.*/TARGET=${SOURCE_BUILD_PROJECT_FILE}/" Makefile
make

if [ "$DEVICE_UUID" == "" ]; then 
    # Generate a UUID
    DEVICE_UUID=$(uuidgen -r | awk -F '-' '{print $NF}' | tr '[:lower:]' '[:upper:]')
    sed -i "s/^DEVICE_UUID=.*/DEVICE_UUID=${DEVICE_UUID}/" .env
fi
if [ "$DEVICE_MAC" == "" ]; then 
    # Generate six pairs of random hexadecimal digits
    DEVICE_MAC=$(printf '%02x:%02x:%02x:%02x:%02x:%02x\n' $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)) $((RANDOM%256)))
    sed -i "s/^DEVICE_MAC=.*/DEVICE_MAC=${DEVICE_MAC}/" .env
fi

CFGF="main.cfg"
echo "[config]" > ${CFGF}
# create configuration file
while IFS='=' read -r name value; do
    if [[ ! -z "$name" && ! -z "$value" ]]; then
        echo "$name=$value" >> $CFGF
    fi
done < ".env"

# edit dynamic variables if they reference another variable inside the .env file
sed -i "s|^DEVICE_VERSION=.*|DEVICE_VERSION=${DEVICE_VERSION}|" $CFGF
sed -i "s|^MQTT_MAIN_TOPIC=.*|MQTT_MAIN_TOPIC=${MQTT_MAIN_TOPIC}|" $CFGF


for ((i = 0; i <= 16; i++)); do # max out to 16 sensors
    sensorName="SENSOR_${i}_NAME" 
    if [ ! -z "${!sensorName}" ]; then
        sensorUid="SENSOR_${i}_UID"
        sensorModel="SENSOR_${i}_MODEL"
        sensorIndex="SENSOR_${i}_INDEX"
        sensorIndexGrp="SENSOR_${i}_INDEX_GROUP"    
        sensorHassMqttTopic="SENSOR_${i}_HASS_MQTT_PUBLISH_TOPIC"
        sensorsQuantity=${i}
        echo "⚙ Configuring ${!sensorName} sensor"
        #(( sensorsQuantity++ ))
        sed -i "s|^SENSOR_${i}_NAME=.*|SENSOR_${i}_NAME=${!sensorName}|" $CFGF
        sed -i "s|^SENSOR_${i}_UID=.*|SENSOR_${i}_UID=${!sensorUid}|" $CFGF
        sed -i "s|^SENSOR_${i}_INDEX=.*|SENSOR_${i}_INDEX=${i}|" $CFGF
        sed -i "s|^SENSOR_${i}_HASS_MQTT_PUBLISH_TOPIC=.*|SENSOR_${i}_HASS_MQTT_PUBLISH_TOPIC=${!sensorHassMqttTopic}|" $CFGF
        
        # if the sensor has the same index group is a master
        if [ ${!sensorIndex} == ${!sensorIndexGrp} ]; then
            echo "🛠 Compiling ${!sensorModel} code"
            sensor_model=$(echo "${!sensorModel}" | tr '[:upper:]' '[:lower:]')
            sensor_conf_file=sensors/${sensor_model}/${sensor_model}.cfg
            sensor_poll_time="SENSOR_${i}_POLL_TIME_MSEC"
            echo "[config]" > "${sensor_conf_file}"
            echo "SENSOR_POLL_TIME_MSEC=${!sensor_poll_time}" >> "${sensor_conf_file}"
            echo "SENSOR_VERSION=${SOURCE_BUILD_PROJECT_BUILD}" >> "${sensor_conf_file}"
            echo "SENSOR_INDEX=${i}" >> "${sensor_conf_file}"
            echo "SENSOR_SHM_KEY=102${i}" >> "${sensor_conf_file}"

            cd "sensors/${sensor_model}/"
            make
            cd ${BUILD_PATH}
            # if build don;t copy to deploy

            cp sensors/${sensor_model}/${sensor_model}.bin deploy/
            cp sensors/${sensor_model}/${sensor_model}.cfg deploy/

            echo "✔ Done"
        fi
    fi
done

sed -i "s|^SENSOR_QUANTITY=.*|SENSOR_QUANTITY=${sensorsQuantity}|" $CFGF
sed -i "s|^SENSOR_QUANTITY=.*|SENSOR_QUANTITY=${sensorsQuantity}|" .env


cp ${SOURCE_BUILD_PROJECT_FILE} deploy/
rm -f ${SOURCE_BUILD_PROJECT_FILE} 
cp -fv .env deploy

mv $CFGF deploy

# just build
if [ "$1" = "build" ]; then exit 0; fi

# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 

if [ ! "$1" = "test" ]; then
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

        # Echo the folder name
        echo "⚙ Adding $lib libraries..."

        if [ ! -d "${libraries}/${lib}/${DEVICE_CHIP}" ]; then
            echo "❌ ${libraries}/${lib}/${DEVICE_CHIP} not found, please add the libraries before continue"
            exit 1
        fi

        COMPRESS_FILE="deploy/${lib}.tar.gz"
        if [ -f "${COMPRESS_FILE}" ]; then
            rm -f ${COMPRESS_FILE}
        fi
        tar -czf ${COMPRESS_FILE} -C ${libraries}/${lib}/${DEVICE_CHIP}/ .

        echo "✔ Done"
    done
    IFS="$OLD_IFS"
fi
# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 


# ⬇ Down
echo "⬆ Uploading Firmware to ${DEVICE_ADDRESS}"
sshpass -p "${DEVICE_SSH_PASSWORD}" scp -r deploy ${DEVICE_ADDRESS}:/root/
echo "✔ Done"
echo "Cleaning build files"
rm -f deploy/*.tar.gz
rm -f deploy/*.bin
rm -f deploy/*.cfg
rm -f deploy/.env
rm -f deploy/$CFGF
echo "✔ Done"

# Check if the first argument is "deploy or test"
if [ "$1" = "test" ]; then
    sshpass -p "${DEVICE_SSH_PASSWORD}" ssh ${DEVICE_ADDRESS} "sh /root/deploy/install.sh test"
else
    sshpass -p "${DEVICE_SSH_PASSWORD}" ssh ${DEVICE_ADDRESS} "sh /root/deploy/install.sh deploy"
fi


if [ ! $? -eq 0 ]; then
    echo "❌ Failed"
    exit 1
fi

sed -i "s/^SOURCE_BUILD_PROJECT_BUILD=.*/SOURCE_BUILD_PROJECT_BUILD=${SOURCE_BUILD_PROJECT_BUILD}/" .env
echo "✔ Build has finished"