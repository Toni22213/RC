// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PACKET_SIZE 2000

static int buildControlPacket(unsigned char *packet, unsigned char type,
                              const char *filename, long fileSize)
{
    int n = 0;
    packet[n++] = type;

    packet[n++] = 0x00;
    packet[n++] = 0x08;
    for (int i = 7; i >= 0; i--)
        packet[n++] = (unsigned char)((fileSize >> (8 * i)) & 0xFF);

    int nameLen = strlen(filename);
    packet[n++] = 0x01;
    packet[n++] = (unsigned char)nameLen;
    memcpy(&packet[n], filename, nameLen);
    n += nameLen;

    return n;
}

static int parseControlPacket(const unsigned char *packet, int len,
                              unsigned char *type, char *filename,
                              long *fileSize)
{
    if (len < 3) return -1;
    *type = packet[0];
    int i = 1;
    while (i < len) {
        unsigned char T = packet[i++];
        if (i >= len) return -1;
        unsigned char L = packet[i++];
        if (i + L > len) return -1;

        if (T == 0x00 && L == 8) {
            long v = 0;
            for (int k = 0; k < 8; k++)
                v = (v << 8) | packet[i + k];
            *fileSize = v;
        } else if (T == 0x01) {
            memcpy(filename, &packet[i], L);
            filename[L] = '\0';
        }
        i += L;
    }
    return 0;
}

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
        if (llOpenTx(llParameters) < 0) {
            fprintf(stderr, "llOpenTx failed\n");
            return;
        }

        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            perror("fopen");
            llCloseTx();
            return;
        }

        fseek(fp, 0, SEEK_END);
        long fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        unsigned char ctrl[MAX_PACKET_SIZE];
        int ctrlLen = buildControlPacket(ctrl, 0x01, filename, fileSize);
        if (llSend(ctrl, ctrlLen) < 0) {
            fprintf(stderr, "Failed to send START\n");
            fclose(fp);
            llCloseTx();
            return;
        }

        int seq = 0;
        long sent = 0;
        unsigned char packet[MAX_PACKET_SIZE];
        while (sent < fileSize) {
            int chunk = (fileSize - sent > MAX_PAYLOAD_SIZE)
                            ? MAX_PAYLOAD_SIZE : (int)(fileSize - sent);
            unsigned char data[MAX_PAYLOAD_SIZE];
            if (fread(data, 1, chunk, fp) != (size_t)chunk) {
                fprintf(stderr, "fread failed\n");
                break;
            }

            int n = 0;
            packet[n++] = 0x00;
            packet[n++] = (unsigned char)(seq & 0xFF);
            packet[n++] = (unsigned char)((seq >> 8) & 0xFF);
            packet[n++] = (unsigned char)(chunk >> 8);
            packet[n++] = (unsigned char)(chunk & 0xFF);
            memcpy(&packet[n], data, chunk);
            n += chunk;

            if (llSend(packet, n) < 0) {
                fprintf(stderr, "llSend failed at seq %d\n", seq);
                fclose(fp);
                llCloseTx();
                return;
            }
            sent += chunk;
            seq = (seq + 1) & 0xFF;
        }
        fclose(fp);

        ctrlLen = buildControlPacket(ctrl, 0x02, filename, fileSize);
        if (llSend(ctrl, ctrlLen) < 0)
            fprintf(stderr, "Failed to send END\n");

        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0)
    {
        if (llOpenRx(llParameters) < 0) {
            fprintf(stderr, "llOpenRx failed\n");
            return;
        }

        char outName[256] = {0};
        long expectedSize = 0;
        FILE *fp = NULL;
        int done = FALSE;

        while (!done) {
            unsigned char packet[MAX_PACKET_SIZE];
            int len = llReceive(packet);
            if (len < 0) {
                fprintf(stderr, "llReceive error\n");
                break;
            }
            if (len == 0) continue;

            unsigned char type = packet[0];

            if (type == 0x01) {
                parseControlPacket(packet, len, &type, outName, &expectedSize);
                fp = fopen(filename, "wb");
                if (!fp) {
                    perror("fopen");
                    break;
                }
                printf("Receiving '%s' (%ld bytes) -> '%s'\n",
                       outName, expectedSize, filename);
            }
            else if (type == 0x02) {
                parseControlPacket(packet, len, &type, outName, &expectedSize);
                done = TRUE;
            }
            else if (type == 0x00) {
                if (len < 5) continue;
                int L = (packet[3] << 8) | packet[4];
                if (5 + L > len) continue;
                if (fp) fwrite(&packet[5], 1, L, fp);
            }
        }

        if (fp) fclose(fp);
        llCloseRx();
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
    }
}
