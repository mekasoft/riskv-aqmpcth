// Ref: https://github.com/technion/lol_dht22/blob/master/dht22.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <wiringx.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 1024

// memoruy allocations
float *dht22_shared_memory;
float *pms5003_shared_memory;
float *mhz19b_shared_memory;
// shared memory variables
float *dht22_mem_temperature;
float *dht22_mem_humidity;

float *pms5003_mem_pm1;
float *pms5003_mem_pm25;
float *pms5003_mem_pm100;
float *pms5003_mem_pm1env;
float *pms5003_mem_pm25env;
float *pms5003_mem_pm10env;
float *pms5003_mem_pd03;
float *pms5003_mem_pd05;
float *pms5003_mem_pd1;
float *pms5003_mem_pd25;
float *pms5003_mem_pd50;
float *pms5003_mem_pd100;

float *mhz19b_mem_co2;
// serial timings
#define MAXTIMINGS 85 // 85?

// config ini variables
#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 256

// STATUS Led on board
int DUO_LED = 25;

//  Configuration Variables from .env file
char MQTT_HOST[16];
char MQTT_PORT[8];
char MQTT_USER[32];
char MQTT_PASS[64];
char HASS_MQTT_SENSOR_TOPIC[128];

char SOURCE_BUILD_PROJECT_MAJOR_VERSION[8];
char SOURCE_BUILD_PROJECT_MINOR_VERSION[8];
char SOURCE_BUILD_PROJECT_BUILD[8];
char DEVICE_VERSION[16];
char HASS_URL[32];
char DEVICE_BOARD_BRAND[32];
char MQTT_PUB_FREQ_MSEC[8];
char SENSOR_MAIN_POLL_TIME_MSEC[8];
char SENSOR_QUANTITY[8];

char software_version[8];

// Define a struct to hold sensor properties
struct Sensor {
    char name[64];
    char model[16];
    char description[64];
    char manufacturer[16];
    char brand[16];
    char class[16];
    char unit[16];
    char poll_time_msec[16];
    char code[16];
    char uid[128];
    char index[16];
    char index_grp[16];
    char hass_mqtt_publish_topic[128];
    char data_type[16];
    char data[64];
};

// declare 16 sensors TODO make this dynamic to save memory
struct Sensor SENSOR[16];
//float SENSOR_DATA[16];

// Function to strip leading and trailing whitespace and quotes from a string
char *strip_quotes(char *str) {
    if (str == NULL) {
        return NULL;
    }

    // Strip leading quotes
    while (*str && (*str == '\'' || *str == '"')) {
        str++;
    }

    int len = strlen(str);
    // Strip trailing quotes
    while (len > 0 && (str[len - 1] == '\'' || str[len - 1] == '"' || str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
    return str;
}

// Function to parse .env file and load variables into global variables
void load_cfg_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("[MAIN] Error opening file");
        return;
    }

    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        char *delimiter = strchr(line, '=');
        if (delimiter) {
            *delimiter = '\0'; // Split line into key and value
            int indx;
            char *key = line;
            char *value = delimiter + 1;
            char sensorKeyName[64];
            char sensorKeyModel[64];
            char sensorKeyDescription[64];
            char sensorKeyManufacturer[64];
            char sensorKeyBrand[64];
            char sensorKeyClass[64];
            char sensorKeyUnit[64];
            char sensorKeyPollTime[64];
            char sensorKeyCode[64];
            char sensorKeyUid[64];
            char sensorKeyMqttTopic[64];
            char sensorDataType[16];
            char sensorIndex[16];
            char sensorIndexGroup[16];

            // Strip leading and trailing whitespace and quotes
            //key = strip_quotes(key);
            value = strip_quotes(value);

            if (sscanf(key, "SENSOR_%d_", &indx) == 1) {
                //printf("[MAIN] DEBUG SENSORS - %s = %s \n",key,value);

                sprintf(sensorKeyName, "SENSOR_%d_NAME", indx);
                sprintf(sensorKeyModel, "SENSOR_%d_MODEL", indx);
                sprintf(sensorKeyDescription, "SENSOR_%d_DESCRIPTION", indx);
                sprintf(sensorKeyManufacturer, "SENSOR_%d_MANUFACTURER", indx);
                sprintf(sensorKeyBrand, "SENSOR_%d_BRAND", indx);
                sprintf(sensorKeyClass, "SENSOR_%d_CLASS", indx);
                sprintf(sensorKeyUnit, "SENSOR_%d_UNIT", indx);
                sprintf(sensorKeyPollTime, "SENSOR_%d_POLL_TIME_MSEC", indx);
                sprintf(sensorKeyCode, "SENSOR_%d_CODE", indx);
                sprintf(sensorKeyUid, "SENSOR_%d_UID", indx);
                sprintf(sensorKeyMqttTopic, "SENSOR_%d_HASS_MQTT_PUBLISH_TOPIC", indx);
                sprintf(sensorIndex, "SENSOR_%d_INDEX", indx);
                sprintf(sensorDataType, "SENSOR_%d_DATA_TYPE", indx);
                sprintf(sensorIndexGroup, "SENSOR_%d_INDEX_GROUP", indx);


                if (strcmp(key, sensorKeyName) == 0) {
                    strncpy(SENSOR[indx].name, value, sizeof(SENSOR[indx].name) - 1);
                    SENSOR[indx].name[sizeof(SENSOR[indx].name) - 1] = '\0';
                    printf("[MAIN] Found Sensor %d %s \n",indx,SENSOR[indx].name);
                } else if (strcmp(key, sensorKeyModel) == 0) {
                    strncpy(SENSOR[indx].model, value, sizeof(SENSOR[indx].model) - 1);
                    SENSOR[indx].model[sizeof(SENSOR[indx].model) - 1] = '\0';
                } else if (strcmp(key, sensorKeyDescription) == 0) {
                    strncpy(SENSOR[indx].description, value, sizeof(SENSOR[indx].description) - 1);
                    SENSOR[indx].description[sizeof(SENSOR[indx].description) - 1] = '\0';
                } else if (strcmp(key, sensorKeyManufacturer) == 0) {
                    strncpy(SENSOR[indx].manufacturer, value, sizeof(SENSOR[indx].manufacturer) - 1);
                    SENSOR[indx].manufacturer[sizeof(SENSOR[indx].manufacturer) - 1] = '\0';
                } else if (strcmp(key, sensorKeyBrand) == 0) {
                    strncpy(SENSOR[indx].brand, value, sizeof(SENSOR[indx].brand) - 1);
                    SENSOR[indx].brand[sizeof(SENSOR[indx].brand) - 1] = '\0';
                } else if (strcmp(key, sensorKeyClass) == 0) {
                    strncpy(SENSOR[indx].class, value, sizeof(SENSOR[indx].class) - 1);
                    SENSOR[indx].class[sizeof(SENSOR[indx].class) - 1] = '\0';
                } else if (strcmp(key, sensorKeyUnit) == 0) {
                    strncpy(SENSOR[indx].unit, value, sizeof(SENSOR[indx].unit) - 1);
                    SENSOR[indx].unit[sizeof(SENSOR[indx].unit) - 1] = '\0';
                } else if (strcmp(key, sensorKeyPollTime) == 0) {
                    strncpy(SENSOR[indx].poll_time_msec, value, sizeof(SENSOR[indx].poll_time_msec) - 1);
                    SENSOR[indx].poll_time_msec[sizeof(SENSOR[indx].poll_time_msec) - 1] = '\0';
                } else if (strcmp(key, sensorKeyCode) == 0) {
                    strncpy(SENSOR[indx].code, value, sizeof(SENSOR[indx].code) - 1);
                    SENSOR[indx].code[sizeof(SENSOR[indx].code) - 1] = '\0';
                } else if (strcmp(key, sensorKeyUid) == 0) {
                    strncpy(SENSOR[indx].uid, value, sizeof(SENSOR[indx].uid) - 1);
                    SENSOR[indx].uid[sizeof(SENSOR[indx].uid) - 1] = '\0';
                } else if (strcmp(key, sensorKeyMqttTopic) == 0) {
                    strncpy(SENSOR[indx].hass_mqtt_publish_topic, value, sizeof(SENSOR[indx].hass_mqtt_publish_topic) - 1);
                    SENSOR[indx].hass_mqtt_publish_topic[sizeof(SENSOR[indx].hass_mqtt_publish_topic) - 1] = '\0';
                } else if (strcmp(key, sensorIndex) == 0) {
                    strncpy(SENSOR[indx].index, value, sizeof(SENSOR[indx].index) - 1);
                    SENSOR[indx].index[sizeof(SENSOR[indx].index) - 1] = '\0';
                } else if (strcmp(key, sensorIndexGroup) == 0) {
                    strncpy(SENSOR[indx].index_grp, value, sizeof(SENSOR[indx].index_grp) - 1);
                    SENSOR[indx].index_grp[sizeof(SENSOR[indx].index_grp) - 1] = '\0';
                } else if (strcmp(key, sensorDataType) == 0) {
                    strncpy(SENSOR[indx].data_type, value, sizeof(SENSOR[indx].data_type) - 1);
                    SENSOR[indx].data_type[sizeof(SENSOR[indx].data_type) - 1] = '\0';
                }


            } else {
                //printf("[MAIN] DEBUG CONFIG - %s = %s \n",key,value);
                // Store key-value pairs into global variables
                if (strcmp(key, "MQTT_HOST") == 0) {
                    strncpy(MQTT_HOST, value, sizeof(MQTT_HOST) - 1);
                    MQTT_HOST[sizeof(MQTT_HOST) - 1] = '\0';
                } else if (strcmp(key, "MQTT_USER") == 0) {
                    strncpy(MQTT_USER, value, sizeof(MQTT_USER) - 1);
                    MQTT_USER[sizeof(MQTT_USER) - 1] = '\0';
                } else if (strcmp(key, "MQTT_PORT") == 0) {
                    strncpy(MQTT_PORT, value, sizeof(MQTT_PORT) - 1);
                    MQTT_PORT[sizeof(MQTT_PORT) - 1] = '\0';
                } else if (strcmp(key, "MQTT_PASS") == 0) {
                    strncpy(MQTT_PASS, value, sizeof(MQTT_PASS) - 1);
                    MQTT_PASS[sizeof(MQTT_PASS) - 1] = '\0';
                } else if (strcmp(key, "HASS_MQTT_SENSOR_TOPIC") == 0) {
                    strncpy(HASS_MQTT_SENSOR_TOPIC, value, sizeof(HASS_MQTT_SENSOR_TOPIC) - 1);
                    HASS_MQTT_SENSOR_TOPIC[sizeof(HASS_MQTT_SENSOR_TOPIC) - 1] = '\0';
                } else if (strcmp(key, "SOURCE_BUILD_PROJECT_MAJOR_VERSION") == 0) {
                    strncpy(SOURCE_BUILD_PROJECT_MAJOR_VERSION, value, sizeof(SOURCE_BUILD_PROJECT_MAJOR_VERSION) - 1);
                    SOURCE_BUILD_PROJECT_MAJOR_VERSION[sizeof(SOURCE_BUILD_PROJECT_MAJOR_VERSION) - 1] = '\0';
                } else if (strcmp(key, "SOURCE_BUILD_PROJECT_MINOR_VERSION") == 0) {
                    strncpy(SOURCE_BUILD_PROJECT_MINOR_VERSION, value, sizeof(SOURCE_BUILD_PROJECT_MINOR_VERSION) - 1);
                    SOURCE_BUILD_PROJECT_MINOR_VERSION[sizeof(SOURCE_BUILD_PROJECT_MINOR_VERSION) - 1] = '\0';
                } else if (strcmp(key, "SOURCE_BUILD_PROJECT_BUILD") == 0) {
                    strncpy(SOURCE_BUILD_PROJECT_BUILD, value, sizeof(SOURCE_BUILD_PROJECT_BUILD) - 1);
                    SOURCE_BUILD_PROJECT_BUILD[sizeof(SOURCE_BUILD_PROJECT_BUILD) - 1] = '\0';
                } else if (strcmp(key, "DEVICE_VERSION") == 0) {
                    strncpy(DEVICE_VERSION, value, sizeof(DEVICE_VERSION) - 1);
                    DEVICE_VERSION[sizeof(DEVICE_VERSION) - 1] = '\0';
                } else if (strcmp(key, "HASS_URL") == 0) {
                    strncpy(HASS_URL, value, sizeof(HASS_URL) - 1);
                    HASS_URL[sizeof(HASS_URL) - 1] = '\0';
                } else if (strcmp(key, "DEVICE_BOARD_BRAND") == 0) {
                    strncpy(DEVICE_BOARD_BRAND, value, sizeof(DEVICE_BOARD_BRAND) - 1);
                    DEVICE_BOARD_BRAND[sizeof(DEVICE_BOARD_BRAND) - 1] = '\0';
                } else if (strcmp(key, "MQTT_PUB_FREQ_MSEC") == 0) {
                    strncpy(MQTT_PUB_FREQ_MSEC, value, sizeof(MQTT_PUB_FREQ_MSEC) - 1);
                    MQTT_PUB_FREQ_MSEC[sizeof(MQTT_PUB_FREQ_MSEC) - 1] = '\0';
                } else if (strcmp(key, "SENSOR_MAIN_POLL_TIME_MSEC") == 0) {
                    strncpy(SENSOR_MAIN_POLL_TIME_MSEC, value, sizeof(SENSOR_MAIN_POLL_TIME_MSEC) - 1);
                    SENSOR_MAIN_POLL_TIME_MSEC[sizeof(SENSOR_MAIN_POLL_TIME_MSEC) - 1] = '\0';
                } else if (strcmp(key, "SENSOR_QUANTITY") == 0) {
                    strncpy(SENSOR_QUANTITY, value, sizeof(SENSOR_QUANTITY) - 1);
                    SENSOR_QUANTITY[sizeof(SENSOR_QUANTITY) - 1] = '\0';
                }
            }
        }
    }

    sprintf(software_version, "%s.%s.%s", SOURCE_BUILD_PROJECT_MAJOR_VERSION,
                                          SOURCE_BUILD_PROJECT_MINOR_VERSION,
                                          SOURCE_BUILD_PROJECT_BUILD);
    fclose(file);
}

void getSensorDataShm() {

    // filter dht22 data and convert to string to publish 
    sprintf(SENSOR[0].data, "%.1f", *dht22_mem_temperature);
    sprintf(SENSOR[1].data, "%.1f", *dht22_mem_humidity);
    printf("[MAIN] GET-SHM - Temperature = %s *C \n",SENSOR[0].data);
    printf("[MAIN] GET-SHM - Humidity = %s %% \n",SENSOR[1].data);

    // filter pms5003 data  and convert to string to publish 
    sprintf(SENSOR[2].data, "%.0f", *pms5003_mem_pm25);
    printf("[MAIN] GET-SHM - PM2.5 = %s μ g/m3 \n",SENSOR[2].data);

    // filter pms5003 data  and convert to string to publish 
    sprintf(SENSOR[3].data, "%.0f", *mhz19b_mem_co2);
    printf("[MAIN] GET-SHM - Co2 = %s ppm \n",SENSOR[3].data);
    

}

int sendSesorRegistryMQTT(int indx) {

    // refs https://www.home-assistant.io/integrations/update.mqtt/
    // https://www.home-assistant.io/integrations/sensor/#device-class
    char json_string[1024];
    snprintf(json_string, sizeof(json_string),
    "{"
    "\"device_class\": \"%s\","
    "\"state_topic\": \"%s\","
    "\"unit_of_measurement\":\"%s\","
    "\"unique_id\": \"%s\","
    "\"retain\": \"true\","
    "\"state_topic\": \"%sstate\","
    "\"device\": {"
        "\"identifiers\": [\"%s\"],"
        "\"name\": \"%s\","
        "\"manufacturer\": \"%s\","
        "\"model\": \"%s\","
        "\"hw_version\": \"%s\","
        "\"sw_version\": \"%s\","
        "\"configuration_url\": \"%s\""
        "}"
    "}", 
    SENSOR[indx].class,SENSOR[indx].hass_mqtt_publish_topic,SENSOR[indx].unit,SENSOR[indx].uid,SENSOR[indx].hass_mqtt_publish_topic,
        SENSOR[indx].code,SENSOR[indx].name,SENSOR[indx].manufacturer,SENSOR[indx].model,DEVICE_VERSION,software_version,HASS_URL);

    char cmd[1024];
    // add the config part to the topic for home assistant autodiscovery
    sprintf(cmd, "mosquitto_pub -h %s -u %s -P %s -p %s -t %sconfig -m \'%s\' -r", MQTT_HOST, MQTT_USER ,MQTT_PASS ,MQTT_PORT, SENSOR[indx].hass_mqtt_publish_topic,json_string);

    //printf("Executing command: %s\n", cmd);
    int result = system(cmd);

    if (result != 0) {
        fprintf(stderr, "[MAIN] Failed to publish message.\n");
        return 1;
    }
    printf("[MAIN] Sensor %d %s registered at mqtt %s \n", indx, SENSOR[indx].name,MQTT_HOST);
    
    // Reset sensor  data
    //SENSOR_DATA[indx]=0;
    //SENSOR[indx].data=0;
    
    return 0;

}

int sendSensorDataMQTT(int indx) {

    char cmd[512];
    sprintf(cmd, "mosquitto_pub -h %s -u %s -P %s -p %s -t %sstate -m %s", MQTT_HOST, MQTT_USER ,MQTT_PASS ,MQTT_PORT, SENSOR[indx].hass_mqtt_publish_topic,SENSOR[indx].data);
    
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "[MAIN] Failed to publish message.\n");
        return 1;
    }
    printf("[MAIN] PUBLISH-MQTT - %s -> %s\n", SENSOR[indx].name, MQTT_HOST);
    return 0;

}

void sharedMemoryControl() {

    // TODO refactor, DRY this code 
    // DHT22 * * * * * * * * * * * 
    int shmid;
    key_t dht22_key = 1020;

    // Locate the shared memory segment
    shmid = shmget(dht22_key, SHM_SIZE, 0666);
    if (shmid == -1) {
        perror("[MAIN] shmget dht22");
            shmid = shmget(dht22_key, SHM_SIZE, IPC_CREAT | 0666);
            if (shmid == -1) {
                perror("[MAIN] shmget dht22");
                exit(1);
            } else {
                printf("[MAIN] Shared Memory Created dht22 \n");
            }
    }

    // Attach to the shared memory
    dht22_shared_memory = (float *)shmat(shmid, NULL, 0);
    if (dht22_shared_memory == (float *)(-1)) {
        perror("[MAIN] shmat dht22");
        exit(1);
    }

    // Assign variables within the shared memory
    dht22_mem_temperature = dht22_shared_memory;
    dht22_mem_humidity = dht22_shared_memory + 1;


    // PMS5003 * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * 
    // Different key for separate shared memory segment
    key_t pms5003_key = 1022;

    // Locate the shared memory segment
    shmid = shmget(pms5003_key, SHM_SIZE, 0666);
    if (shmid == -1) {
        perror("[MAIN] shmget pms5003");
            shmid = shmget(pms5003_key, SHM_SIZE, IPC_CREAT | 0666);
            if (shmid == -1) {
                perror("[MAIN] shmget pms5003");
                exit(1);
            } else {
                printf("[MAIN] Shared Memory Created pms5003 \n");
            }
    }

    // Attach to the shared memory
    pms5003_shared_memory = (float *)shmat(shmid, NULL, 0);
    if (pms5003_shared_memory == (float *)(-1)) {
        perror("[MAIN] shmat pms5003");
        exit(1);
    }

    // Assign variables within the shared memory
    pms5003_mem_pm1 = pms5003_shared_memory;
    pms5003_mem_pm25 = pms5003_shared_memory + 1;
    pms5003_mem_pm100 = pms5003_shared_memory + 2;
    pms5003_mem_pm1env = pms5003_shared_memory + 3;
    pms5003_mem_pm25env = pms5003_shared_memory + 4;
    pms5003_mem_pm10env = pms5003_shared_memory + 5;
    pms5003_mem_pd03 = pms5003_shared_memory + 6;
    pms5003_mem_pd05 = pms5003_shared_memory + 7;
    pms5003_mem_pd1 = pms5003_shared_memory + 8;
    pms5003_mem_pd25 = pms5003_shared_memory + 9;
    pms5003_mem_pd50 = pms5003_shared_memory + 10;
    pms5003_mem_pd100 = pms5003_shared_memory + 11;


    // mhz19b * * * * * * ** * * * * * * * * * *  * * * * * * * * *  
    // Different key for separate shared memory segment
    key_t mhz19b_key = 1023;

    // Locate the shared memory segment
    shmid = shmget(mhz19b_key, SHM_SIZE, 0666);
    if (shmid == -1) {
        perror("[MAIN] shmget mhz19b");
            shmid = shmget(mhz19b_key, SHM_SIZE, IPC_CREAT | 0666);
            if (shmid == -1) {
                perror("[MAIN] shmget mhz19b");
                exit(1);
            } else {
                printf("[MAIN] Shared Memory Created mhz19b \n");
            }
    }

    // Attach to the shared memory
    mhz19b_shared_memory = (float *)shmat(shmid, NULL, 0);
    if (mhz19b_shared_memory == (float *)(-1)) {
        perror("[MAIN] shmat mhz19b");
        exit(1);
    }

    // Assign variables within the shared memory
    mhz19b_mem_co2 = mhz19b_shared_memory;


}

int main() {

    // LOAD variables from .env file
    load_cfg_file("/root/main.cfg");
    printf("[MAIN] Starting main v%s \n",software_version);

    // PREPARE HArdware
   if(wiringXSetup("duo", NULL) == -1) {
        wiringXGC();
        return -1;
    }

    if(wiringXValidGPIO(DUO_LED) != 0) {
        printf("[MAIN] Invalid GPIO %d\n", DUO_LED);
    }

    pinMode(DUO_LED, PINMODE_OUTPUT);
    digitalWrite(DUO_LED, LOW);

    // wait to other programs to boot and fill shared memory
    delayMicroseconds(100000);
    sharedMemoryControl();

    // REGISTER SENSORS HASS-MQTT
    int i;
    int sensorQty = atoi(SENSOR_QUANTITY);
    for (i = 0; i <= sensorQty; i++) {
        sendSesorRegistryMQTT(i);
    }

    // MAIN LOOP
    int limit_poll_sensor = atoi(SENSOR_MAIN_POLL_TIME_MSEC);
    int limit_poll_mqtt = atoi(MQTT_PUB_FREQ_MSEC);
    
    int counter_sync_mqtt = 0;
    int counter_led = 0;
    int counter_sensor_data = 0;

    while (1) {
        // loop every 10 milliseconds
        delayMicroseconds(100000);
        // Increment the counter every time the loop runs
        counter_sync_mqtt++;
        counter_led++;
        counter_sensor_data++;

        // 10 = i sec
        if (counter_led >= 10) {
            counter_led = 0; // Reset the counter
            digitalWrite(DUO_LED, HIGH);
            delayMicroseconds(100);
        }

        if (counter_sensor_data == limit_poll_sensor) {
            counter_sensor_data = 0; // Reset the counter
            digitalWrite(DUO_LED, HIGH);
            getSensorDataShm();
        }
        
        // every 10 seconds
        if (counter_sync_mqtt == limit_poll_mqtt ) {
            counter_sync_mqtt = 0; // Reset the counter
            digitalWrite(DUO_LED, HIGH);
            // iterate all sensors
            for (i = 0; i <= sensorQty; i++) {
                sendSensorDataMQTT(i);
            }
        }

    digitalWrite(DUO_LED, LOW);

    }

    shmdt(dht22_shared_memory);
    shmdt(pms5003_shared_memory);
    shmdt(mhz19b_shared_memory);
    return 0;
}