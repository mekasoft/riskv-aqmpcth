// Ref: https://github.com/technion/lol_dht22/blob/master/dht22.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <wiringx.h>
#include <sys/stat.h>
#include <sys/shm.h>

#define SHM_SIZE 1024
float *dhtt22_mem_temperature;
float *dhtt22_mem_humidity;
float *shared_memory;

// serial timings
#define MAXTIMINGS 85 // 85?
// config ini variables
#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 256

static int DHTPIN = 16;
static int dht22_dat[5] = {0, 0, 0, 0, 0};

//char SENSOR_FIFO[16];
char SENSOR_POLL_TIME_MSEC[8];
char SENSOR_VERSION[8];
char SENSOR_SHM_KEY[8];

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

// Function to parse config file and load variables 
void load_cfg_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("[DHT22] Error opening file");
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

            // Strip leading and trailing whitespace and quotes
            //key = strip_quotes(key);
            value = strip_quotes(value);

                // Store key-value pairs into global variables
            if (strcmp(key, "SENSOR_POLL_TIME_MSEC") == 0) {
                strncpy(SENSOR_POLL_TIME_MSEC, value, sizeof(SENSOR_POLL_TIME_MSEC) - 1);
                SENSOR_POLL_TIME_MSEC[sizeof(SENSOR_POLL_TIME_MSEC) - 1] = '\0';
            } else if (strcmp(key, "SENSOR_VERSION") == 0) {
                strncpy(SENSOR_VERSION, value, sizeof(SENSOR_VERSION) - 1);
                SENSOR_VERSION[sizeof(SENSOR_VERSION) - 1] = '\0';
            } else if (strcmp(key, "SENSOR_SHM_KEY") == 0) {
                strncpy(SENSOR_SHM_KEY, value, sizeof(SENSOR_SHM_KEY) - 1);
                SENSOR_SHM_KEY[sizeof(SENSOR_SHM_KEY) - 1] = '\0';
            }   
          
        }
    }
    fclose(file);
}

static uint8_t sizecvt(const int read) {
    /* digitalRead() and friends from wiringpi are defined as returning a value
    < 256. However, they are returned as int() types. This is a safety function */

    if (read > 255 || read < 0) {
        printf("[DHT22] Invalid data from wiringx library\n");
        exit(EXIT_FAILURE);
    }
    return (uint8_t)read;

}

static int get_sensor_data() {

    uint8_t laststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;

    dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

    // pull pin down for 18 milliseconds
    pinMode(DHTPIN, PINMODE_OUTPUT);
    digitalWrite(DHTPIN, HIGH);
    // delay(500);
    delayMicroseconds(500000);
    digitalWrite(DHTPIN, LOW);
    // delay(20);
    delayMicroseconds(20000);
    // prepare to read the pin
    pinMode(DHTPIN, PINMODE_INPUT);

    // detect change and read data
    for (i = 0; i < MAXTIMINGS; i++) {
        counter = 0;
        while (sizecvt(digitalRead(DHTPIN)) == laststate) {
            counter++;
            delayMicroseconds(2);
            if (counter == 255) {
                break;
            }
        }
        laststate = sizecvt(digitalRead(DHTPIN));

        if (counter == 255)
            break;

        // ignore first 3 transitions
        if ((i >= 4) && (i % 2 == 0)) {
            // shove each bit into the storage bytes
            dht22_dat[j / 8] <<= 1;
            if (counter > 16)
                dht22_dat[j / 8] |= 1;
            j++;
        }
    }


    if ((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))) {
        float t, h;
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F) * 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)
            t *= -1;
        
        // Write to the shared memory
        *dhtt22_mem_temperature = t;
        *dhtt22_mem_humidity = h;

        printf("[DHT22] PUT-SHM - Temperature = %.2f *C Humidity = %.2f %% \n", t, h);
        return 1;

    } else {
        printf("[DHT22] DHT22 Offline?\n");
        return 0;
    }
    
}

void shared_memory_control() {
    int shmid;
    key_t key = atoi(SENSOR_SHM_KEY);

    // Create the shared memory segment
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    } else {
        printf("[DHT22] Shared Memory Created \n");
    }

    // Attach to the shared memory
    shared_memory = (float *)shmat(shmid, NULL, 0);
    if (shared_memory == (float *)(-1)) {
        perror("shmat");
        exit(1);
    } else {
        printf("[DHT22] Shared Memory Attached \n");
    }

    // Assign variables within the shared memory
    dhtt22_mem_temperature = shared_memory;
    dhtt22_mem_humidity = shared_memory + 1;
}

int main() {

    // LOAD variables from .cfg file
    load_cfg_file("/root/dht22.cfg");
    printf("[DHT22] Starting dht22 v%s \n",SENSOR_VERSION);

    // PREPARE HArdware
   if(wiringXSetup("duo", NULL) == -1) {
        wiringXGC();
        return -1;
    }

    if (wiringXValidGPIO(DHTPIN) != 0) {
        printf("[DHT22] Invalid GPIO %d\n", DHTPIN);
    }

    shared_memory_control();

    // MAIN LOOP
    int limit_poll_sensor = atoi(SENSOR_POLL_TIME_MSEC);
    int counter = 999;

    while (1) {
        // loop every 10 milliseconds
        // Increment the counter every time the loop runs
        counter++;
        delayMicroseconds(100000);

        if (counter >= limit_poll_sensor) {
            counter = 0; // Reset the counter
            get_sensor_data();
        }

    }

    shmdt(shared_memory);

    return 0;
}