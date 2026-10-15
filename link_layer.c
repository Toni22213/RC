// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "alarm_sigaction.h"

#include <stdio.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

#define FLAG 0x7E
#define A_TX_RX 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define SUP_FRAME_SIZE 5

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
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

    // Create string to send
    unsigned char set[SUP_FRAME_SIZE] = {FLAG, A_TX_RX, C_SET, A_TX_RX ^ C_SET, FLAG};

    int bytes = writeBytesSerialPort(set, SUP_FRAME_SIZE);
    printf("%d bytes written to serial port\n", bytes);

    if (bytes != SUP_FRAME_SIZE)
    {
        perror("writeBytesSerialPort");
        return -1;
    }
    alarmSetup();
    int alarmEnabled = FALSE;
    int alarmCount = 0;


    // Wait until all bytes have been written to the serial port
    unsigned char ua[SUP_FRAME_SIZE] = {0};
    while (alarmCount < TRIES +1)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(TIME); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }

        int r = readByteSerialPort(&ua);
        if (r < 0)
        {
            perror("readByteSerialPort");
            return -1;
        }
        if (r == 0) continue;

        
        if ()
        {#define C_DISC 0x0B
            alarm(0);
            break;
        }
        if (TRIGGER)
        {
            llSend(set, SUP_FRAME_SIZE);
            TRIGGER = FALSE;
            alarmEnabled = FALSE;
            alarmCount++;
        }
    }
    
    

    // Close serial port
    printf("Connection established as Tx\n");

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

    unsigned char set[SUP_FRAME_SIZE] = {0};
    llReceive(set);

    // Create string to send
    unsigned char ua[SUP_FRAME_SIZE] = {FLAG, A_TX_RX, C_UA, A_TX_RX ^ C_UA, FLAG};
    llSend(ua, SUP_FRAME_SIZE);

    // Close serial port
    printf("Connection established as Rx\n");

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    int bytes = writeBytesSerialPort(buf, bufSize);
    printf("%d bytes written to serial port\n", bytes);

    if (bytes != bufSize)
    {
        perror("writeBytesSerialPort");
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    volatile int STOP = FALSE;
    int nBytesBuf = 0;
    int started = FALSE;
    while (STOP == FALSE)
    {
        unsigned char byte;
        int r = readByteSerialPort(&byte);
        if (r < 0)
        {
            perror("readByteSerialPort");
            return -1;
        }
        if (r == 0) continue;
        nBytesBuf += r;

        printf("var =  0x%02X\n", byte);
        if (nBytesBuf <= SUP_FRAME_SIZE)
        {
            switch (nBytesBuf)
            {
                case 1:
                    if (byte != FLAG)
                    {
                        printf("Invalid frame. Restart reading from serial port.\n");
                        nBytesBuf = 0;
                    }
                    break;
                case 2:
                    if (byte != A_TX_RX)
                    {
                        if (byte == FLAG)
                        {
                            nBytesBuf = 1;
                        }
                        else
                        {
                            printf("Invalid frame. Restart reading from serial port.\n");
                            nBytesBuf = 0;
                        }
                    }
                    break;
                case 3:
                    if (byte != C_SET && byte != C_UA && byte != C_DISC)
                    {
                        if (byte == FLAG)
                        {
                            nBytesBuf = 1;
                        }
                        else
                        {
                            printf("Invalid frame. Restart reading from serial port.\n");
                            nBytesBuf = 0;
                        }
                    }
                    break;
                case 4:
                    if (byte != (A_TX_RX ^ C_SET) && byte != (A_TX_RX ^ C_UA))
                    {
                        if (byte == FLAG)
                        {
                            nBytesBuf = 1;
                        }
                        else
                        {
                            printf("Invalid frame. Restart reading from serial port.\n");
                            nBytesBuf = 0;
                        }
                    }
                    break;
                case 5:
                    if (byte != FLAG)
                    {
                        printf("Invalid frame. Restart reading from serial port.\n");
                        nBytesBuf = 0;
                    }
                    else
                    {
                        if (started == FALSE)
                        {
                            started = TRUE;
                        }
                        printf("Received frame. Stop reading from serial port.\n");
                        STOP = TRUE;
                    }
                    break;
            }
            packet[nBytesBuf - 1] = byte;
        }
    }
    printf("Total bytes received: %d\n", nBytesBuf);

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx(){  
    unsigned char disc[SUP_FRAME_SIZE] = {
        FLAG, A_TX_RX, C_DISC, A_TX_RX ^ C_DISC, FLAG
    };

    unsigned char response[SUP_FRAME_SIZE] = {0};

    // Send DISC to request disconnection
    if (llSend(disc, SUP_FRAME_SIZE) < 0)
        return -1;

    printf("DISC sent. Waiting for UA...\n");

    // Wait for the receiver's response
    if (llReceive(response) < 0)
        return -1;

    // Verify that the response is a valid UA frame
    if (response[0] != FLAG ||
        response[1] != A_TX_RX ||
        response[2] != C_UA ||
        response[3] != (A_TX_RX ^ C_UA) ||
        response[4] != FLAG)
    {
        printf("Invalid UA frame received.\n");
        return -1;
    }

    printf("UA received. Connection closed as Tx.\n");

    // Close the serial port
    if (closeSerialPort() < 0)
        {
        perror("closeSerialPort");
        return -1;
    }

    return 0;
}

int llCloseRx()
{
   unsigned char frame[SUP_FRAME_SIZE] = {0};

    printf("Waiting for DISC...\n");

    // Wait for the transmitter's DISC frame
    if (llReceive(frame) < 0)
        return -1;

    // Verify the received DISC frame
    if (frame[0] != FLAG ||
        frame[1] != A_TX_RX ||
        frame[2] != C_DISC ||
        frame[3] != (A_TX_RX ^ C_DISC) ||
        frame[4] != FLAG)
    {
        printf("Invalid DISC frame received.\n");
        return -1;
    }

    // Build and send UA
    unsigned char ua[SUP_FRAME_SIZE] = {
        FLAG, A_TX_RX, C_UA, A_TX_RX ^ C_UA, FLAG
    };

    if (llSend(ua, SUP_FRAME_SIZE) < 0)
        return -1;

    printf("UA sent. Connection closed as Rx.\n");

    // Close the serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    return 0;
}
