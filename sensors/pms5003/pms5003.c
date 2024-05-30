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
float *shared_memory;
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

// serial timings
#define MAXTIMINGS 85 // 85?
// config ini variables
#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 256

char SENSOR_POLL_TIME_MSEC[8];
char SENSOR_VERSION[8];
char SENSOR_SHM_KEY[8];
int pmf_data[12];


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
        perror("[PMS5003] Error opening file");
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

void shared_memory_control() {
    int shmid;
    key_t key = atoi(SENSOR_SHM_KEY);

    // Create the shared memory segment
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    } else {
        printf("[PMS5003] Shared Memory Created \n");
    }

    // Attach to the shared memory
    shared_memory = (float *)shmat(shmid, NULL, 0);
    if (shared_memory == (float *)(-1)) {
        perror("shmat");
        exit(1);
    } else {
        printf("[PMS5003] Shared Memory Attached \n");
    }

    // Assign variables within the shared memory
    pms5003_mem_pm1 = shared_memory;
    pms5003_mem_pm25 = shared_memory + 1;
    pms5003_mem_pm100 = shared_memory + 2;
    pms5003_mem_pm1env = shared_memory + 3;
    pms5003_mem_pm25env = shared_memory + 4;
    pms5003_mem_pm10env = shared_memory + 5;
    pms5003_mem_pd03 = shared_memory + 6;
    pms5003_mem_pd05 = shared_memory + 7;
    pms5003_mem_pd1 = shared_memory + 8;
    pms5003_mem_pd25 = shared_memory + 9;
    pms5003_mem_pd50 = shared_memory + 10;
    pms5003_mem_pd100 = shared_memory + 11;

}

void get_sensor_data(int fd) {
    // https://milkv.io/docs/duo/application-development/wiringx#uart-usage-example
    // https://github.com/vogelrh/pms5003c/blob/master/pms5003.h
    // https://how2electronics.com/interfacing-pms5003-air-quality-sensor-arduino/

    //char buf[1024];
    int str_len = 0;
    int i;

    // copy the logic from 
   //https://github.com/szajakubiak/Python_PMS5003/blob/master/pms5003.py
    int data_receive = 0;
    int data_high = 0;
    int data_low = 0;


    wiringXSerialFlush(fd);

    wiringXSerialPutChar(fd,0x42);
    wiringXSerialPutChar(fd,0x4D);
    wiringXSerialPutChar(fd,0xE2);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x01);
    wiringXSerialPutChar(fd,0x71);

    while(1){
        /* in ACTIVE mode If the concentration change is small the sensor 
        would run at stable mode with the real interval of 2.3s.And if the change is 
        big the sensor would be changed to fast mode automatically with the 
        interval of 200~800ms*/
        delayMicroseconds(1000);

        str_len = wiringXSerialDataAvail(fd);
        if (str_len > 0) {

            printf("[PMS5003] START UART SCAN * * * * * \n");
            printf("[PMS5003] LENGHT %d\n", str_len);

            delayMicroseconds(10);
            data_receive = wiringXSerialGetChar(fd);
            printf("[PMS5003] BUFFER %d\n", data_receive);

            if (data_receive == 0x42){
                printf("[PMS5003] START 0x42: %d \n", data_receive);
                data_receive = wiringXSerialGetChar(fd);
                if (data_receive == 0x4d){
                    printf("[PMS5003] START 0x4D: %d \n", data_receive);
                    delayMicroseconds(10);
                    str_len = wiringXSerialDataAvail(fd);
                    printf("[PMS5003] DATA LENGHT %d\n", str_len);

                    // Frame length=2x13+2(data+check bytes)
                    data_receive = wiringXSerialGetChar(fd);
                    printf("[PMS5003] FRAME HIGH: %d \n", data_receive);
                    data_receive = wiringXSerialGetChar(fd);
                    printf("[PMS5003] FRAME LOW: %d \n", data_receive);

                    for (i = 0; i <= 12; i++) {
                        data_high = wiringXSerialGetChar(fd);
                        data_low = wiringXSerialGetChar(fd);
                        pmf_data[i] = (data_high * 256 + data_low);
                        printf("[PMS5003] DATA %d: %d\n", i, pmf_data[i]);

                    }

                    *pms5003_mem_pm1 = pmf_data[0]; //PM1.0 concentration unit μ g/m3（CF=1，standard particle
                    *pms5003_mem_pm25 = pmf_data[1]; //PM2.5 concentration unit μ g/m3（CF=1，standard particle） << STANDARD DATA
                    *pms5003_mem_pm100 = pmf_data[2]; //PM10 concentration unit μ g/m3（CF=1，standard particle）
                    *pms5003_mem_pm1env = pmf_data[3]; //PM1.0 concentration unit μ g/m3（under atmospheric environment 
                    *pms5003_mem_pm25env = pmf_data[4]; //PM2.5 concentration unit μ g/m3（under atmospheric environment
                    *pms5003_mem_pm10env = pmf_data[5]; //PM10 concentration unit (under atmospheric environment) μ g/m3 
                    *pms5003_mem_pd03 = pmf_data[6]; //number of particles with diameter beyond  0.3 um in   0.1 L of air.
                    *pms5003_mem_pd05 = pmf_data[7]; //number of particles with diameter beyond  0.5 um in   0.1 L of air.
                    *pms5003_mem_pd1 = pmf_data[8]; //number of particles with diameter beyond  1.0 um in   0.1 L of air. 
                    *pms5003_mem_pd25 = pmf_data[9];//number of particles with diameter beyond  2.5 um in   0.1 L of air.
                    *pms5003_mem_pd50 = pmf_data[10]; //number of particles with diameter beyond  5.0 um in   0.1 L of air. 
                    *pms5003_mem_pd100 = pmf_data[11]; //number of particles with diameter beyond  10 um in   0.1 L of air.

                    // checksum Check code=Start character 1+ Start character 2 +……..+ data 13   Low 8 bits 
                    data_receive = wiringXSerialGetChar(fd);
                    printf("[PMS5003] CHECK HIGH: %d \n", data_receive);
                    data_receive = wiringXSerialGetChar(fd);
                    printf("[PMS5003] CHECK LOW: %d \n", data_receive);

                    printf("[PMS5003] PUT-SHM - STANDARDS PM2.5 = %d \n", pmf_data[1]);
                    break;
                    
                }
            }
        }
    }
    return;
}

int main() {

    // LOAD variables from .env file
    load_cfg_file("/root/pms5003.cfg");
    printf("[PMS5003] Starting pms5003 v%s \n",SENSOR_VERSION);

    // PREPARE HArdware
   if(wiringXSetup("duo", NULL) == -1) {
        wiringXGC();
        return -1;
    }

    shared_memory_control();

    // You can assign GPIO 4-5 to either UART 2 or 3 2 is recommended
    // UART 1 and 3 is not enabled
    // UART 4 and 0 are available
    struct wiringXSerial_t wiringXSerial = {9600, 8, 'n', 1, 'n'};
    int fd = -1;
    if ((fd = wiringXSerialOpen("/dev/ttyS4", wiringXSerial)) < 0) {
        printf("[PMS5003] Open serial device failed: %d\n", fd);
        fprintf(stderr, "[PMS5003] Unable to open serial device: %s\n", strerror(errno));
        wiringXGC();
        return -1;
    }
    char message[] = {0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70};

    delayMicroseconds(1000);
    wiringXSerialPutChar(fd,0x42);
    wiringXSerialPutChar(fd,0x4D);
    wiringXSerialPutChar(fd,0xE1);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x01);
    wiringXSerialPutChar(fd,0x70);

    // Send the entire message in one command
    //wiringXSerialPuts(fd, message);

    // MAIN LOOP
    int limit_poll_sensor = atoi(SENSOR_POLL_TIME_MSEC);
    // force the first read
    int counter = 999;

    while (1) {
        // loop every 10 milliseconds
        // Increment the counter every time the loop runs
        counter++;
        delayMicroseconds(100000);

        if (counter >= limit_poll_sensor) {
            counter = 0; // Reset the counter
            get_sensor_data(fd);
        }

    }
    wiringXSerialClose(fd);
    shmdt(shared_memory);

    return 0;
}



