// Link layer protocol implementation

#include "link_layer.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1  // POSIX compliant source

LinkLayer connectionParameters;

// Alarm
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal) {
    alarmCount++;
    alarmEnabled = TRUE;

    printf("Alarm Enabled! \n");
}

void alarmDisable() {
    alarm(0);
    alarmEnabled = FALSE;
    alarmCount = 0;
}

int receiveFrame(uint8_t expectedAddress, uint8_t expectedControl) {
    t_state state = START;

    printf("\n Waiting for bytes to read... \n");

    while (state != STOP) {
        uint8_t buf = 0;
        int bytes = readByteSerialPort(&buf);

        if (bytes < 0) {
            printf("Error trying to read frame! \n");
            return -1;
        }

        if (bytes > 0) {
            printf("Read byte: 0x%02X \n", buf);

            switch (state) {
                case START:
                    if (buf == FLAG)
                        state = FLAG_RCV;
                    break;

                case FLAG_RCV:
                    if (buf == FLAG)
                        continue;
                    if (buf == expectedAddress)
                        state = A_RCV;
                    else
                        state = START;
                    break;

                case A_RCV:
                    if (buf == expectedControl)
                        state = C_RCV;
                    else if (buf == FLAG)
                        state = FLAG_RCV;
                    else
                        state = START;
                    break;

                case C_RCV:
                    if (buf == (expectedControl ^ expectedAddress))
                        state = BCC_OK;
                    else if (buf == FLAG)
                        state = FLAG_RCV;
                    else
                        state = START;
                    break;

                case BCC_OK:
                    if (buf == FLAG)
                        state = STOP;
                    else
                        state = START;
                    break;

                default:
                    state = START;
            }

            /*
            if (state == START)
              printf("Sent back to start! \n");

            if (state == FLAG_RCV)
              printf("Read flag! \n");

            if (state == A_RCV)
              printf("Read address! \n");

            if (state == C_RCV)
              printf("Read controller! \n");

            if (state == BCC_OK)
              printf("Read BCC! \n");

            if (state == STOP) {
              printf("Read the last flag! \n");
              printf("Correctly read the header! \n");
            }
            */
        }
    }

    return 0;
}

int transmitFrame(uint8_t expectedAddress, uint8_t expectedControl, t_frame_type frameType) {
    t_state state = START;

    (void)signal(SIGALRM, alarmHandler);

    if (writeBytesSerialPort(frame_buffers[frameType], BUF_SIZE) < 0)
        return -1;

    printf("Wrote frame! \n");

    alarm(connectionParameters.timeout);

    while (state != STOP && alarmCount <= connectionParameters.nRetransmissions) {
        uint8_t buf = 0;
        int bytes = readByteSerialPort(&buf);

        if (bytes < 0) {
            printf("Error trying to read frame! \n");
            return -1;
        }

        if (bytes > 0) {
            printf("Read byte: 0x%02X \n", buf);

            switch (state) {
                case START:
                    if (buf == FLAG)
                        state = FLAG_RCV;
                    break;

                case FLAG_RCV:
                    if (buf == FLAG)
                        continue;
                    if (buf == expectedAddress)
                        state = A_RCV;
                    else
                        state = START;
                    break;

                case A_RCV:
                    if (buf == expectedControl)
                        state = C_RCV;
                    else if (buf == FLAG)
                        state = FLAG_RCV;
                    else
                        state = START;
                    break;

                case C_RCV:
                    if (buf == (expectedControl ^ expectedAddress))
                        state = BCC_OK;
                    else if (buf == FLAG)
                        state = FLAG_RCV;
                    else
                        state = START;
                    break;

                case BCC_OK:
                    if (buf == FLAG)
                        state = STOP;
                    else
                        state = START;
                    break;

                default:
                    state = START;
            }
        }

        if (state == STOP) {
            alarmDisable();
            return 0;
        }

        if (alarmEnabled) {
            alarmEnabled = FALSE;

            if (alarmCount <= connectionParameters.nRetransmissions) {
                if (writeBytesSerialPort(frame_buffers[frameType], BUF_SIZE) < 0)
                    return -1;

                printf("Retransmiting frame! \n");
                alarm(connectionParameters.timeout);
            }

            state = START;
        }
    }

    alarmDisable();
    return 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

int llopen(LinkLayer connection) {
    memcpy(&connectionParameters, &connection, sizeof(connection));

    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
        return -1;

    switch (connectionParameters.role) {
        case LlTx:
            if (transmitFrame(ADDR_SEND, CTRL_UA, SET))
                return -1;
            printf("Connected! \n");
            break;
        case LlRx:
            if (receiveFrame(ADDR_SEND, CTRL_SET))
                return -1;
            if (writeBytesSerialPort(frame_buffers[UA_Rx], BUF_SIZE) < 0)
                return -1;
            printf("Connected! \n");
            break;
    }

    return 0;
}

const uint8_t *byteStuffing(const uint8_t *buf, int bufSize, int *stuffedSize) {
    if (buf == NULL || stuffedSize == NULL)
        return NULL;

    uint8_t *ret = malloc(2 * bufSize * sizeof(uint8_t) + 1);
    int stuffedIndex = 0;

    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE) {
            ret[stuffedIndex++] = ESCAPE;
            ret[stuffedIndex++] = buf[i] ^ ESCAPE_OFFSET;
            continue;
        }

        ret[stuffedIndex++] = buf[i];
    }

    *stuffedSize = stuffedIndex;
    ret = realloc(ret, stuffedIndex * sizeof(uint8_t));

    if (ret == NULL)
        return NULL;

    return ret;
}

uint8_t *byteDestuffing(const uint8_t *buf, int bufSize, int *destuffedSize) {
    if (buf == NULL || destuffedSize == NULL)
        return NULL;

    uint8_t *ret = malloc(bufSize * sizeof(uint8_t));
    int destuffedIndex = 0;

    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == ESCAPE && i + 1 < bufSize) {
            ret[destuffedIndex++] = buf[++i] ^ ESCAPE_OFFSET;
            continue;
        }

        ret[destuffedIndex++] = buf[i];
    }

    *destuffedSize = destuffedIndex;
    ret = realloc(ret, destuffedIndex * sizeof(uint8_t));

    if (ret == NULL)
        return NULL;

    return ret;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *packet, int packetSize) {
    static int frameNumber = 0;

    if (packet == NULL) return NULL;

    int stuffedSize = 0;
    const uint8_t *stuffedPacket = byteStuffing(packet, packetSize, &stuffedSize);
    if (stuffedPacket == NULL) return -1;

    uint8_t *frame = malloc(stuffedSize + 7 * sizeof(uint8_t));

    if (frame == NULL) return (free(stuffedPacket), -1);

    frame[0] = FLAG;
    frame[1] = ADDR_SEND;
    frame[2] = frameNumber ? CTRL_INFO0 : CTRL_INFO1;
    frame[3] = frame[1] ^ frame[2];
    memcpy(frame + 4, stuffedPacket, stuffedSize);

    uint8_t bcc = 0;
    for (int i = 0; i < packetSize; i++)
        bcc ^= packet[i];

    frame[4 + stuffedSize] = bcc;

    if (bcc == FLAG || bcc == ESCAPE) {
        frame[4 + stuffedSize] = ESCAPE;
        frame[5 + stuffedSize] = bcc ^ ESCAPE_OFFSET;
        stuffedSize++;
    }

    frame[5 + stuffedSize] = FLAG;

    t_state state = START;
    (void)signal(SIGALRM, alarmHandler);

    if (writeBytesSerialPort(frame, stuffedSize + 6) < 0)
        return (free(stuffedPacket), free(frame), -1);

    alarm(connectionParameters.timeout);

    uint8_t receivedAddr = 0, receivedCtrl = 0;
    while (state != STOP && alarmCount <= connectionParameters.timeout) {
        uint8_t byte = 0;
        int bytes = readByteSerialPort(&byte);

        if (bytes < 0)
            return (free(stuffedPacket), free(frame), -1);

        if (bytes > 0) {
            switch (state) {
                case START:
                    receivedAddr = 0;
                    receivedCtrl = 0;
                    if (byte == FLAG)
                        state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == FLAG)
                        continue;
                    if (byte == ADDR_SEND || byte == A_RCV) {
                        state = A_RCV;
                        receivedAddr = byte;
                    } else
                        state = START;
                    break;
                case A_RCV:
                    if (byte == CTRL_RR0 || byte == CTRL_RR1 || byte) {
                        state = C_RCV;
                        receivedCtrl = byte;
                    } else if (byte == FLAG)
                        state = FLAG_RCV;
                    else
                        state = START;
                    break;
                case C_RCV:
                    if (byte == (CTRL_RR0 ^ ADDR_SEND) || byte == (CTRL_RR1 ^ ADDR_SEND))
                        state = BCC_OK;
                    else if (byte == FLAG)
                        state = FLAG_RCV;
                    else
                        state = START;
                    break;
                case BCC_OK:
                    if (byte == FLAG)
                        state = STOP;
                    else
                        state = START;
                    break;
                default:
                    state = START;
            }
        }

        if (state == STOP) {
        }

        if (alarmEnabled) {
            alarmEnabled = FALSE;

            if (alarmCount <= connectionParameters.nRetransmissions) {
                if (writeBytesSerialPort(frame, stuffedSize + 6) < 0)
                    return (free(stuffedPacket), free(frame), -1);

                alarm(connectionParameters.timeout);
            }

            state = START;
        }

        alarmDisable();
        return (free(stuffedPacket), free(frame), -1);
    }

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
    switch (connectionParameters.role) {
        case LlTx:
            if (transmitFrame(ADDR_RCV, CTRL_DISC, DISC_Tx))
                break;
            if (writeBytesSerialPort(frame_buffers[UA_Tx], BUF_SIZE) < 0)
                break;
            printf("Disconnected! \n");
            break;
        case LlRx:
            if (receiveFrame(ADDR_SEND, CTRL_DISC))
                break;
            if (transmitFrame(ADDR_RCV, CTRL_UA, DISC_Rx))
                break;
            printf("Disconnected! \n");
            break;
    }

    return closeSerialPort();
}
