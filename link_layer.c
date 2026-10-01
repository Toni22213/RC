// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <stdio.h>
#include <unistd.h>

#include <signal.h>
#include <errno.h>


// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

#define TIMEOUT 3
#define MAX_RETRANSMISSIONS 3

volatile sig_atomic_t alarmEnabled = FALSE;
volatile sig_atomic_t alarmCount = 0;


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////


void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d received\n", alarmCount);
}


int writeSET(LinkLayer llParameters){
 unsigned char buf[BUF_SIZE] = {0};
    
    buf[0] = 0x7E;
    buf[1] = 0x03;
    buf[2] = 0x03;
    buf[3] = buf[1]^buf[2];
    buf[4] = 0x7E;

    int bytes = writeBytesSerialPort(buf, BUF_SIZE);
    sleep(1);

    printf("%d bytes written to serial port\n", bytes);
    
    return 0;
    
}

int llOpenTx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and send a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    // Create string to send
   
    // Wait until all bytes have been written to the serial port
    writeSET(llParameters);    

    volatile int STOP = FALSE;
    int nBytesBuf = 0;
    volatile int firstF = FALSE;
    int check[] = {0x7E, 0X03, 0X03, 0X03^0X03, 0X7E};
    int index = 0;
    int retransmissions = 0;

    while (STOP == FALSE && retransmissions < MAX_RETRANSMISSIONS)
    {
        
        alarmEnabled = TRUE;
        alarm(TIMEOUT);

        while (alarmEnabled && STOP == FALSE)
        {
            
            unsigned char byte;
            int bytes = readByteSerialPort(&byte);
            nBytesBuf += bytes;
            
            if(byte == check[index]){
            }
            else if(byte != check[index]){
            printf("error byte mismatch");
            STOP = TRUE;        
            }

            printf("var = 0x%02X\n", (unsigned int)(byte & 0xFF));


            if (!firstF && byte == 0x7E)
            {
            firstF = TRUE;
            }
            else if(firstF && byte == 0x7E){
                STOP = TRUE;
            }
            index++;
        }
    }
    alarm(0);
    alarmEnabled = FALSE;

    if (STOP == FALSE){
        retransmissions++;
        printf("TIMEOUT!! Retransmission %d\n", retransmissions);
        index = 0;
        firstF = FALSE;
        writeSET(llParameters);
    }

    if (STOP == TRUE){
        printf("Frame received successfully");
    }
    


    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port %s closed\n", llParameters.serialPort);

    return 0;
}

int llOpenRx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and receive a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    // Read from serial port until the 'z' char is received.

    // NOTE: This while() cycle is a simple example showing how to read from the serial port.
    // It must be changed in order to respect the specifications of the protocol indicated in the Lab guide.

    // TODO: Save the received bytes in a buffer array and print it at the end of the program.
    volatile int STOP = FALSE;
    int nBytesBuf = 0;
    volatile int firstF = FALSE;
    int check[] = {0x7E, 0X03, 0X03, 0X03^0X03, 0X7E};
    int index = 0;

    while (STOP == FALSE)
    {
        // Read one byte from serial port.
        // NOTE: You must check how many bytes were actually read by reading the return value.
        // In this example, we assume that the byte is always read, which may not be true.
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);
        nBytesBuf += bytes;
        
        if(byte == check[index]){
        }
        else if(byte != check[index]){
        printf("error byte mismtch");
        STOP = TRUE;        
        }

        printf("var = 0x%02X\n", (unsigned int)(byte & 0xFF));


        if (!firstF && byte == 0x7E)
        {
           firstF = TRUE;
        }
        else if(firstF && byte == 0x7E){
            STOP = TRUE;
        }
        index++;

    }
    


    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port %s closed\n", llParameters.serialPort);

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    // TODO: Implement this function

    return 0;
}

int llCloseRx()
{
    // TODO: Implement this function

    return 0;
}
