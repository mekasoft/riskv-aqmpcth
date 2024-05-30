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
float *mhz19b_mem_co2;


// serial timings
#define MAXTIMINGS 20 // 85?
// config ini variables
#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 256

char SENSOR_POLL_TIME_MSEC[8];
char SENSOR_VERSION[8];
char SENSOR_SHM_KEY[8];
int co2f_data[1];
char READ_TYPE[] = "PWM";

static int PWM_PIN = 15;


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
        perror("[MHZ19B] Error opening file");
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
        printf("[MHZ19B] Shared Memory Created \n");
    }

    // Attach to the shared memory
    shared_memory = (float *)shmat(shmid, NULL, 0);
    if (shared_memory == (float *)(-1)) {
        perror("shmat");
        exit(1);
    } else {
        printf("[MHZ19B] Shared Memory Attached \n");
    }

    // Assign variables within the shared memory
    mhz19b_mem_co2 = shared_memory;

}

void get_sensor_data(int fd) {
    // https://milkv.io/docs/duo/application-development/wiringx#uart-usage-example


    //char buf[1024];
    int str_len = 0;
    int data_receive = 0;
    int data_high = 0;
    int data_low = 0;
    int counter = 0;
    
    /* RESPONSE TIMMING

    9.4ms: Transmit 9 Bytes at 9600 baud
    3.2ms: MH-Z19B to process command
    9.3ms: Return response 9 Bytes at 9600 baud

    */



    printf("[MHZ19B] UART ROUTINE \n");
    wiringXSerialFlush(fd);
    wiringXSerialPutChar(fd,0xFF);
    wiringXSerialPutChar(fd,0x01);
    wiringXSerialPutChar(fd,0x86);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x00);
    wiringXSerialPutChar(fd,0x79);
    delayMicroseconds(22000);

    while(1){
        if (counter > MAXTIMINGS) {
            printf("[MHZ19B] UART TIMEOUT %d\n",counter);
            //wiringXSerialFlush(fd);
            break;
        } else {
            counter++;
        }

        delayMicroseconds(1000);

        str_len = wiringXSerialDataAvail(fd);


        if (str_len > 0) {

            printf("[MHZ19B] LENGHT %d\n", str_len);

            data_receive = wiringXSerialGetChar(fd); // 0xFF
            printf("[MHZ19B] BUFFER: %d \n", data_receive); 
            printf("[MHZ19B] START UART SCAN * * * * * \n");
            printf("[MHZ19B] LENGHT %d\n", str_len);
            
            data_receive = wiringXSerialGetChar(fd); // 0x86
            printf("[MHZ19B] BUFFER: %d \n", data_receive);
            
            //data_receive = wiringXSerialGetChar(fd); // 0x01
            //printf("[MHZ19B] BUFFER: %d \n", data_receive);

            //data_receive = wiringXSerialGetChar(fd);
            //printf("[MHZ19B] BUFFER %d\n", data_receive);

            data_high = wiringXSerialGetChar(fd);
            data_low = wiringXSerialGetChar(fd);
            co2f_data[0] = (data_high * 256 + data_low);
            printf("[MHZ19B] DATA %d: %d\n", 0, co2f_data[0]);

            *mhz19b_mem_co2 = co2f_data[0]; //PM1.0 concentration unit μ g/m3（CF=1，standard particle

            // checksum Check code=Start character 1+ Start character 2 +……..+ data 13   Low 8 bits 
            data_receive = wiringXSerialGetChar(fd);
            printf("[MHZ19B] CHECKSUM: %d \n", data_receive);

            printf("[MHZ19B] PUT-SHM - Co2 = %d \n", co2f_data[0]);

            //wiringXSerialFlush(fd);
            //return;
            break;
        }



    }

    return;
}

int get_sensor_data_pwm()
{

    //unsigned int pulse_start, pulse_end;
    unsigned int counter=0;
    unsigned int period=1003000; // mllsecs

    printf("[MHZ19B] Reading PWM Pulse duration\n");
    while (1) {

        // if its low enter the loop
        if (digitalRead(PWM_PIN) == LOW){
        
            while (digitalRead(PWM_PIN) == HIGH) {
                counter++;
                delayMicroseconds(100);
                if (counter > period + 100000) {
                    printf("[MHZ19B] Timeout\n");
                    break;
                }
            }
            if (counter > 0) {
                break;
            }
        }
        //delayMicroseconds(1);
    }    

   
    float ppm=0;
    unsigned int adj=70;
    printf("[MHZ19B] PWM duration: %u microseconds\n", (counter / 5) - 10);
    ppm = ((counter / 5) - 10) * 4 + adj;

    // discard any false readings
    if ( ppm < 4000 ) {

        *mhz19b_mem_co2 = ppm; //PM1.0 concentration unit μ g/m3（CF=1，standard particle

        printf("[MHZ19B] PUT-SHM - Co2 = %d \n", ppm);
    }
    return 0;
}


int main() {

    // LOAD variables from .env file
    load_cfg_file("/root/mhz19b.cfg");
    printf("[MHZ19B] Starting mhz19b v%s \n",SENSOR_VERSION);

    // PREPARE HArdware
   if(wiringXSetup("duo", NULL) == -1) {
        wiringXGC();
        return -1;
    }

    shared_memory_control();
    
    int fd = -1;
    if (READ_TYPE=="UART") {
        //  duo-pinmux -l to looks at the GPO configurations
        // You can assign GPIO 4-5 to either UART 2 or 3 2 is recommended
        // UART 1 and 3 is not enabled
        // UART 4 and 0 are available
        struct wiringXSerial_t wiringXSerial = {9600, 8, 'n', 1, 'n'};
        if ((fd = wiringXSerialOpen("/dev/ttyS0", wiringXSerial)) < 0) {
            printf("[MHZ19B] Open serial device failed: %d\n", fd);
            fprintf(stderr, "[MHZ19B] Unable to open serial device: %s\n", strerror(errno));
            wiringXGC();
            return -1;
        }
    } else {
        // is PWM    
        if (wiringXValidGPIO(PWM_PIN) != 0) {
            printf("[MHZ19B] Invalid GPIO %d\n", PWM_PIN);
        }

        pinMode(PWM_PIN, PINMODE_INPUT);  
    }

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
            if (READ_TYPE=="UART"){
                get_sensor_data(fd);
            } else {
                get_sensor_data_pwm();
            }
        }

    }
    //wiringXSerialClose(fd);
    shmdt(shared_memory);

    return 0;
}



