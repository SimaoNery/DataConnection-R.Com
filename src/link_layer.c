// Link layer protocol implementation

#include "link_layer.h"

#include <signal.h>
#include <stdlib.h>

#include "protocol.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1  // POSIX compliant source

////////////////////////////////////////////////
// ALARM
////////////////////////////////////////////////
int alarmEnabled = FALSE;
int alarmCount = -1;
void (*alarmCallout)(void) = NULL;

void setAlarm(int timeout, int retryNum, void (*callout)(void)) {
    if (timeout < 1 || retryNum < 0 || callout == NULL)
        return;

    alarmEnabled = TRUE;
    alarmCount = retryNum;
    alarmCallout = callout;

    (void)signal(SIGALRM, alarmCallout);

    walarmCallout();
    while (alarmCount - 1) {
        if (alarmEnabled == FALSE)
            break;

        alarm(timeout);
    }
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

void llopenTxHandler();

int llopen(LinkLayer connectionParameters) {
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0) {
        return -1;
    }

    switch (connectionParameters.role) {
        case LlTx:
            setAlarm(connectionParameters.timeout, connectionParameters.nRetransmissions, llopenTxHandler);
            break;
        case LlRx:
            unsigned char buf;
            unsigned char ret_buf[BUF_SIZE] = {FLAG, ADDR_RX, CTRL_UA, ADDR_RX ^ CTRL_UA, FLAG};
            t_state_header state = START;
            while (state != STOP) {
                int bytes = readByteSerialPort(&buf);
                if (bytes <= 0) {
                    perror("read error or unexpected eof");
                    exit(-1);
                }

                switch (state) {
                    case START:
                        state = buf == FLAG ? FLAG_RCV : START;
                        break;
                    case FLAG_RCV:
                        state = buf == ADDR_TX ? A_RCV : buf == FLAG ? FLAG_RCV
                                                                     : START;
                        break;
                    case A_RCV:
                        state = buf == CTRL_SET ? C_RCV : buf == FLAG ? FLAG_RCV
                                                                      : START;
                        break;
                    case C_RCV:
                        state = buf == (ADDR_TX ^ CTRL_SET) ? BCC_OK : buf == FLAG ? FLAG_RCV
                                                                                   : START;
                        break;
                    case BCC_OK:
                        state = buf == FLAG ? STOP : START;
                        break;
                }

                if (state == START)
                    printf("got back start\n");
                if (state == FLAG_RCV)
                    printf("got a flag\n");

                if (state == STOP) {
                    writeBytesSerialPort(ret_buf, BUF_SIZE);
                    printf("Correctly received header\n");
                }
            }
            break;
    }

    return 1;
}

void llopenTxHandler() {
    int bytes = writeBytesSerialPort(txSetBuffer, BUF_SIZE);

    unsigned char buf;
    t_state_header state = START;
    while (state != STOP) {
        int bytes = readByteSerialPort(&buf);
        if (bytes <= 0) {
            perror("read error or unexpected eof");
            exit(-1);
        }

        switch (state) {
            case START:
                state = buf == FLAG ? FLAG_RCV : START;
                break;
            case FLAG_RCV:
                state = buf == ADDR_TX ? A_RCV : buf == FLAG ? FLAG_RCV
                                                             : START;
                break;
            case A_RCV:
                state = buf == CTRL_UA ? C_RCV : buf == FLAG ? FLAG_RCV
                                                             : START;
                break;
            case C_RCV:
                state = buf == (ADDR_TX ^ CTRL_UA) ? BCC_OK : buf == FLAG ? FLAG_RCV
                                                                          : START;
                break;
            case BCC_OK:
                state = buf == FLAG ? STOP : START;
                break;
        }

        if (state == START)
            printf("got back start\n");
        if (state == FLAG_RCV)
            printf("got a flag\n");

        if (state == STOP) {
            alarmEnabled = FALSE;
            printf("Correctly received header\n");
        }
    }
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
