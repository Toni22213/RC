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

static int ns = 0;
static int nr = 0;


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////


void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d received\n", alarmCount);
}

int stuffByte(unsigned char byte, unsigned char *out){
    if (byte == 0x7E) {out[0] = 0x7D; out[1] = 0x5E; return 2;}
    if (byte == 0x7D) {out[0] = 0x7D; out[1] = 0x5D; return 2;}
    out[0] = byte; return 1;
}

static int writeFrame(const unsigned char *body, int bodyLen){
    unsigned char out[BUF_SIZE];
    int n = 0;

    out[n++] = 0x7E;
    for (int i = 0; i < bodyLen; i++){
        n += stuffByte(body[i], &out[n]);
    }
    out[n++] = 0x7E;
    int bytes = writeBytesSerialPort(out, n);
    printf("%d bytes written to serial port \n", bytes);
    return bytes;



}

static int readFrame(unsigned char *body, int maxLen){
    int sawEscape = FALSE;
    int started = FALSE;
    int idx = 0;

    while (1){
        unsigned char byte;
        if (readByteSerialPort(&byte) <= 0) continue;

        if (sawEscape) {
            byte ^= 0x20;
            sawEscape = FALSE;
            if (started) body[idx++] = byte;
            continue;
        }
        if (byte == 0x7D) {sawEscape = TRUE; continue;}
        if (byte == 0x7E) {
            if(!started) {started = TRUE; continue;}
            return idx;
        }
        if (started) body[idx++] = byte;
    }
}


static int buildBody(unsigned char *body, unsigned char a, unsigned char c, const unsigned char *data, int dataLen){
    int n = 0;
    body[n++] = a;
    body[n++] = c;
    body[n++] = a ^ c;

    if (dataLen == 0){
        return 0;
    }

    unsigned char bcc2 = 0;
    for (int i = 0; i < dataLen; i++){
        body[n++] = data[i];
        bcc2 ^= data[i];
    }
    body[n++] = bcc2;
    return n;

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
        return -1;
    }

    // Create string to send
   
    // Wait until all bytes have been written to the serial port

    unsigned char body[3];
    int n = buildBody(body, 0x03, 0x03, NULL, 0);
    int retries = 0, gotUA = FALSE;

    while (!gotUA && retries <= MAX_RETRANSMISSIONS)
    {

        writeFrame(body, n);
        
        alarmEnabled = TRUE;
        alarm(TIMEOUT);

        while (alarmEnabled && !gotUA)
        {
            unsigned char r[BUF_SIZE];
            int len = readFrame(r, BUF_SIZE);
            if(len >= 3 && r[0] == 0x03 && r[1] == 0x07 && r[2] == (r[0] ^r[1])){
                gotUA = TRUE;
            }            
           
        }
    
    alarm(0);
    alarmEnabled = FALSE;

    if (!gotUA){retries++; printf("TIMEOUT %d\n", retries);}
    }

    


    // Close serial port
    if(!gotUA) {closeSerialPort(); return -1;}

    pritnf("UA received\n");
    ns = 0; nr = 0;
    

    return 0;
}

int llOpenRx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and receive a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0){
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);



    while(1){
        unsigned char r[BUF_SIZE];
        int len = readFrame(r, BUF_SIZE);
        if (len < 3) continue;
        if(r[0] == 0x03 && r[1] == 0x03 && r[2] == (r[0] ^r[1])){
            unsigned char ua[3];
            int un = buildBody(ua, 0x03, 0x07, NULL, 0);
            writeFrame(ua,un);
            printf("SET received, UA sent\n");
            break;
        }
    }

    ns = 0; nr = 0;
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
