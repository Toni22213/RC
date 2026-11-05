// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
//#include "alarm_sigaction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <errno.h>


// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 5000

#define FLAG 0x7E
#define ESC  0x7D

#define A_TX 0x03
#define A_RX 0x01

#define C_SET  0x03
#define C_DISC 0x0B
#define C_UA   0x07
#define C_RR(N)  (0x01 | (((N) & 1) << 6))
#define C_REJ(N) (0x05 | (((N) & 1) << 6))
#define C_I(N)   (0x00 | (((N) & 1) << 6))

static int gTimeout = 3;
static int gMaxRetries = 3;

volatile sig_atomic_t alarmEnabled = FALSE;
volatile sig_atomic_t alarmCount = 0;

static int ns = 0;
static int nr = 0;

static unsigned long statFramesSent = 0;
static unsigned long statFramesReceived = 0;
static unsigned long statRetransmissions = 0;
static unsigned long statRejSent = 0;
static unsigned long statTimeouts = 0;

static void alarmHandler(int sig)
{
    (void)sig;
    alarmEnabled = FALSE;
    alarmCount++;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////


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
    if (bytes > 0) statFramesSent++;
    return bytes;
}

static int readFrame(unsigned char *body, int maxLen){
    int sawEscape = FALSE;
    int started = FALSE;
    int idx = 0;

    while (1){
        if (!alarmEnabled) return -1;
        unsigned char byte;
        if (readByteSerialPort(&byte) <= 0) continue;

        if (!started) {
            if (byte == 0x7E) { started = TRUE; idx = 0; }
            continue;
        }

        if (sawEscape) {
            byte ^= 0x20;
            sawEscape = FALSE;
            if (idx < maxLen) body[idx++] = byte;
            continue;
        }
        if (byte == 0x7D) {sawEscape = TRUE; continue;}
        if (byte == 0x7E) {
            if (idx > 0) { statFramesReceived++; return idx; }
            idx = 0;
            continue;
        }
        if (idx < maxLen) body[idx++] = byte;
    }
}


static int buildControlBody(unsigned char *body, unsigned char a, unsigned char c){
    body[0] = a;
    body[1] = c;
    body[2] = a ^ c;
    return 3;
}

static int buildIBody(unsigned char *body, unsigned char a, unsigned char c, const unsigned char *data, int dataLen){
    int n = 0;
    body[n++] = a;
    body[n++] = c;
    body[n++] = a ^ c;

    unsigned char bcc2 = 0;
    for (int i = 0; i < dataLen; i++){
        body[n++] = data[i];
        bcc2 ^= data[i];
    }
    body[n++] = bcc2;
    return n;
}

static int sendControlFrame(unsigned char a, unsigned char c){
    unsigned char body[3];
    int n = buildControlBody(body, a, c);
    return writeFrame(body, n);
}

static int matchControl(unsigned char expA, unsigned char expC, const unsigned char *body, int len){
    return (len >= 3 &&
            body[0] == expA &&
            body[1] == expC &&
            body[2] == (unsigned char)(body[0] ^ body[1]));
}

typedef struct { int wantNs; } AckCtx;

static int matchAck(const unsigned char *body, int len, void *ctx){
    AckCtx *ac = (AckCtx *)ctx;
    if (len < 3) return 0;
    if (body[0] != A_TX) return 0;
    if (body[2] != (unsigned char)(body[0] ^ body[1])) return 0;

    unsigned char c = body[1];
    int isRR  = ((c & 0x0F) == 0x01);
    int isREJ = ((c & 0x0F) == 0x05);
    int num   = (c >> 6) & 1;

    if (isRR  && num == ((ac->wantNs + 1) & 1)) return 1;
    if (isREJ && num == ac->wantNs)             return 1;
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

    gTimeout    = llParameters.timeout;
    gMaxRetries = llParameters.nRetransmissions;

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        closeSerialPort();
        return -1;
    }

    for (int attempt = 0; attempt <= gMaxRetries; attempt++)
    {
        if (sendControlFrame(A_TX, C_SET) < 0) continue;
        if (attempt > 0) statRetransmissions++;

        unsigned char r[BUF_SIZE];
        int len = 0;
        int gotUA = FALSE;

        alarmEnabled = TRUE;
        alarm(gTimeout);

        while (alarmEnabled && !gotUA)
        {
            len = readFrame(r, BUF_SIZE);
            if (len < 0) break; // Interrupted by alarm, break the loop to trigger timeout

            if (len >= 3 && matchControl(A_TX, C_UA, r, len))
                gotUA = TRUE;
        }

        alarm(0);
        alarmEnabled = FALSE;

        if (gotUA)
        {
            ns = 0; nr = 0;
            printf("UA received\n");
            return 0;
        }

        statTimeouts++;
        printf("TIMEOUT %d\n", attempt + 1);
    }

    closeSerialPort();
    return -1;
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

    gTimeout    = llParameters.timeout;
    gMaxRetries = llParameters.nRetransmissions;

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        closeSerialPort();
        return -1;
    }
    alarmEnabled = TRUE;
    while(1){
        unsigned char r[BUF_SIZE];
        int len = readFrame(r, BUF_SIZE);
        if (len < 3) continue;
        if(matchControl(A_TX, C_SET, r, len)){
            sendControlFrame(A_TX, C_UA);
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
    if (bufSize < 0 || bufSize > MAX_PAYLOAD_SIZE + 5) return -1;

    unsigned char body[BUF_SIZE];
    int n = buildIBody(body, A_TX, C_I(ns), buf, bufSize);
    int frameNs = ns;

    for (int attempt = 0; attempt <= gMaxRetries; attempt++)
    {
        if (writeFrame(body, n) < 0) continue;
        if (attempt > 0) statRetransmissions++;

        AckCtx ac = { .wantNs = frameNs };
        unsigned char r[BUF_SIZE];
        int len = 0;
        int gotResponse = FALSE;
        int isRej = FALSE;

        alarmEnabled = TRUE;
        alarm(gTimeout);

        while (alarmEnabled && !gotResponse)
        {
            len = readFrame(r, BUF_SIZE);
            if (len < 0) break;
            if (len < 3) continue;
            if (matchAck(r, len, &ac)) {
                gotResponse = TRUE;
                if ((r[1] & 0x0F) == 0x05) isRej = TRUE;
            }
        }

        alarm(0);
        alarmEnabled = FALSE;

        if (!gotResponse)
        {
            statTimeouts++;
            printf("llSend timeout %d\n", attempt + 1);
            continue;
        }

        if (isRej)
        {
            printf("REJ received, retransmitting N(S)=%d\n", frameNs);
            continue;
        }

        ns ^= 1;
        return 0;
    }

    fprintf(stderr, "llSend failed after retries\n");
    return -1;
}


////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////

int llReceive(unsigned char *packet)
{
    alarmEnabled = TRUE;
    while (1)
    {
        unsigned char r[BUF_SIZE];
        int len = readFrame(r, BUF_SIZE);

         if (len < 0) continue;

        // Need at least A, C, BCC1, one data byte, BCC2
        if (len < 5)
            continue;

        // Check header: A must be Tx, BCC1 valid, must be I-frame
        if (r[0] != A_TX || r[2] != (unsigned char)(r[0] ^ r[1]))
            continue;
        if ((r[1] & 0x01) != 0x00)
            continue;

        int frameNs = (r[1] >> 6) & 1;

        // Check BCC2 (payload integrity)
        int dlen = len - 4;
        unsigned char bcc2 = 0;
        for (int i = 0; i < dlen; i++)
            bcc2 ^= r[3 + i];

        if (bcc2 != r[3 + dlen])
        {
            // Corrupt -> send REJ requesting retransmission, don't touch nr
            sendControlFrame(A_TX, C_REJ(nr));
            statRejSent++;
            printf("BCC2 error -> REJ(%d) sent\n", nr);
            continue;
        }

        // Check if this is the I-frame we expect (N(S) == nr)
        if (frameNs == nr)
        {
            // Valid in-order frame: deliver data
            for (int i = 0; i < dlen; i++)
                packet[i] = r[3 + i];

            nr ^= 1;

            // Send RR with new nr
            sendControlFrame(A_TX, C_RR(nr));
            return dlen;
        }
        else
        {
            // Duplicate: re-send RR with current nr, don't deliver
            sendControlFrame(A_TX, C_RR(nr));
            printf("Duplicate I-frame N(S)=%d -> RR(%d)\n", frameNs, nr);
            continue;
        }
    }
}


////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    unsigned char r[BUF_SIZE];
    int len = 0;
    int gotDisc = FALSE;

    for (int attempt = 0; attempt <= gMaxRetries; attempt++)
    {
        sendControlFrame(A_TX, C_DISC);
        if (attempt > 0) statRetransmissions++;

        gotDisc = FALSE;
        alarmEnabled = TRUE;
        alarm(gTimeout);

        while (alarmEnabled && !gotDisc)
        {
            len = readFrame(r, BUF_SIZE);
            if (len < 0) break; // Interrupted by alarm

            // Rx sends DISC back. We must reply with UA and close.
            if (len >= 3 && matchControl(A_TX, C_DISC, r, len)) {
                sendControlFrame(A_TX, C_UA);
                gotDisc = TRUE;
            }
        }

        alarm(0);
        alarmEnabled = FALSE;

        if (gotDisc) {
            printf("DISC received, UA sent, closing Tx\n");
            break;
        }
        statTimeouts++;
        printf("DISC timeout %d\n", attempt + 1);
    }

    printf("Statistics (Tx):\n");
    printf("  Frames sent: %lu\n", statFramesSent);
    printf("  Frames received: %lu\n", statFramesReceived);
    printf("  Retransmissions: %lu\n", statRetransmissions);
    printf("  Timeouts: %lu\n", statTimeouts);

    closeSerialPort();
    return gotDisc ? 0 : -1;
}

int llCloseRx()
{
    unsigned char r[BUF_SIZE];
    int len = 0;
    int gotUa = FALSE;

    // 1. Wait for Tx to send DISC
    while (1)
    {
        len = readFrame(r, BUF_SIZE);
        if (len < 0) continue;

        if (len >= 3 && matchControl(A_TX, C_DISC, r, len))
        {
            // 2. Reply with DISC
            sendControlFrame(A_TX, C_DISC);
            
            // 3. Wait for UA from Tx
            alarmEnabled = TRUE;
            alarm(gTimeout);

            while (alarmEnabled && !gotUa)
            {
                len = readFrame(r, BUF_SIZE);
                if (len < 0) break;
                if (len >= 3 && matchControl(A_TX, C_UA, r, len))
                    gotUa = TRUE;
            }

            alarm(0);
            alarmEnabled = FALSE;
            
            printf("DISC received, DISC sent, connection closed\n");
            break;
        }
    }

    printf("Statistics (Rx):\n");
    printf(" Frames sent: %lu\n", statFramesSent);
    printf(" Frames received: %lu\n", statFramesReceived);
    printf(" REJ sent: %lu\n", statRejSent);
    printf(" Timeouts: %lu\n", statTimeouts);

    closeSerialPort();
    return 0;
}
