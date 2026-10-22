// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer llParameters = {
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout,
    };
    strcpy(llParameters.serialPort, serialPort);

    if (strcmp(role, "tx") == 0)
    {
        if (llOpenTx(llParameters) < 0)
            return;

        // Send the string as the payload
        unsigned char buf[MAX_PAYLOAD_SIZE];
        int len = strlen(filename);
        memcpy(buf, filename, len);

        llSend(buf, len);

        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0)
    {
        if (llOpenRx(llParameters) < 0)
            return;

        // Receive the payload
        unsigned char packet[MAX_PAYLOAD_SIZE];
        int len = llReceive(packet);

        printf("Received %d bytes: %.*s\n", len, len, packet);

        llCloseRx();
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
    }
}
