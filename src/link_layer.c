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
#define _POSIX_SOURCE 1 // POSIX compliant source

#define SET_Command newSUFrame(ADDR_SEND, CTRL_SET)
#define UA_Rx_Response newSUFrame(ADDR_SEND, CTRL_UA)
#define UA_Tx_Response newSUFrame(ADDR_RCV, CTRL_UA)
#define DISC_Rx_Command newSUFrame(ADDR_RCV, CTRL_DISC)
#define DISC_Tx_Command newSUFrame(ADDR_SEND, CTRL_DISC)
#define RR0_Command newSUFrame(ADDR_SEND, CTRL_RR0)
#define RR1_Command newSUFrame(ADDR_SEND, CTRL_RR1)
#define REJ0_Command newSUFrame(ADDR_SEND, CTRL_REJ0)
#define REJ1_Command newSUFrame(ADDR_SEND, CTRL_REJ1)

LinkLayer connectionParameters;
int frameNumber = 0;

// Alarm
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmCount++;
    alarmEnabled = TRUE;
}

void alarmDisable()
{
    alarm(0);
    alarmEnabled = FALSE;
    alarmCount = 0;
}

t_frame *newFrame(t_frame_addr addr, t_frame_ctrl ctrl, uint8_t *data, size_t dataSize)
{
    if ((ctrl == CTRL_INFO0 || ctrl == CTRL_INFO1) && data == NULL)
        return printf("INFO frames require data fields\n"), NULL;

    t_frame *ret = malloc(sizeof(t_frame));
    if (ret == NULL)
        return printf("Couldn't allocate memory for frame.\n"), NULL;

    ret->a = addr;
    ret->c = ctrl;
    ret->bcc1 = addr ^ ctrl;

    ret->bytesToStuff = 0;
    ret->bcc2 = 0x00;
    for (size_t i = 0; i < dataSize; i++)
    {
        if (data[i] == FLAG || data[i] == ESCAPE)
            ret->bytesToStuff += 1;
        ret->bcc2 ^= data[i];
    }

    if (ret->bcc2 == FLAG || ret->bcc2 == FLAG)
        ret->bytesToStuff += 1;

    ret->dataSize = dataSize;
    ret->data = data;

    return ret;
}

uint8_t *frameToString(t_frame *frame, size_t *finalSize)
{
    if (frame == NULL)
        return printf("Can't convert NULL frame\n"), NULL;

    if (finalSize == NULL)
        return printf("Can't save final size to NULL pointer\n"), NULL;

    int isInfoFrame = frame->c == CTRL_INFO0 || frame->c == CTRL_INFO1;

    uint8_t *ret = calloc(5 + isInfoFrame + frame->bytesToStuff + frame->dataSize, sizeof(uint8_t));
    if (ret == NULL)
        return printf("Couldn't allocate memory for stuffed string\n"), NULL;

    size_t index = 0;
    ret[index++] = FLAG;
    ret[index++] = frame->a;
    ret[index++] = frame->c;
    ret[index++] = frame->bcc1;

    if (isInfoFrame == FALSE)
    {
        ret[index++] = FLAG;
        *finalSize = index;
        return ret;
    }

    // Stuff data
    for (size_t i = 0; i < frame->dataSize; i++)
    {
        uint8_t c = frame->data[i];
        if (c == FLAG || c == ESCAPE)
        {
            ret[index++] = ESCAPE;
            ret[index++] = c ^ ESCAPE_OFFSET;
            continue;
        }
        ret[index++] = c;
    }

    if (frame->bcc2 == FLAG || frame->bcc2 == ESCAPE)
    {
        ret[index++] = ESCAPE;
        ret[index++] = frame->bcc2 ^ ESCAPE_OFFSET;
    }
    else
    {
        ret[index++] = frame->bcc2;
    }

    ret[index++] = FLAG;
    *finalSize = index;

    return ret;
}

t_frame *newSUFrame(t_frame_addr addr, t_frame_ctrl ctrl)
{
    return newFrame(addr, ctrl, NULL, 0);
}

int writeFrameToSerialPort(t_frame *frame, int endFree)
{
    if (frame == NULL)
        return printf("Can't write NULL frame\n"), -1;
    
    size_t size = 0;
    uint8_t *string = frameToString(frame, &size);
    if (string == NULL)
    {
        if (endFree) free(frame);
        return -1;
    }

    int retv = writeBytesSerialPort(string, size);
    if (endFree) free(frame);
    
    return free(string), retv;
}

int    frameDestuff(t_frame *frame)
{
    if (frame == NULL)
        return printf("Can't destuff NULL frame\n"), -1;
    
    if (frame->data == NULL)
        return printf("Can't destuff NULL data\n"), -1;
    
    size_t stuffedSize = frame->dataSize;

    size_t index = 0;
    for (int i = 0; i < stuffedSize; i++)
    {
        uint8_t c = frame->data[i];
        if (c == ESCAPE && i < stuffedSize - 1)
        {
            i++;
            frame->data[index++] = frame->data[i] ^ ESCAPE_OFFSET;
            continue;
        }

        frame->data[index++] = frame->data[i];
    }

    frame->bcc2 = frame->data[index - 1];
    frame->dataSize = index - 1;

    return 0;
}

int receiveFrame(uint8_t expectedAddress, uint8_t expectedControl)
{
    t_state state = START;

    printf("\n Waiting for retv to read... \n");

    while (state != STOP)
    {
        uint8_t buf = 0;
        int retv = readByteSerialPort(&buf);

        if (retv < 0)
        {
            printf("Error trying to read frame! \n");
            return -1;
        }

        if (retv > 0)
        {
            switch (state)
            {
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
    }

    return 0;
}

int transmitFrame(t_frame_addr sentAddr, t_frame_ctrl sentCtrl,
                    t_frame_addr expectedAddr, t_frame_ctrl expectedCtrl)
{
    t_state state = START;

    (void)signal(SIGALRM, alarmHandler);

    t_frame *frame = newSUFrame(sentAddr, sentCtrl);
    if (frame == NULL)
        return -1;

    size_t size = 0;
    uint8_t *frameString = frameToString(frame, &size);
    if (frameString == NULL)
        return free(frame), -1;
    if (writeBytesSerialPort(frameString, size) < 0)
        return free(frame), free(frameString), -1;

    printf("Wrote frame! \n");

    alarm(connectionParameters.timeout);

    while (state != STOP && alarmCount <= connectionParameters.nRetransmissions)
    {
        uint8_t buf = 0;
        int retv = readByteSerialPort(&buf);

        if (retv < 0)
        {
            printf("Error trying to read frame! \n");
            return -1;
        }

        if (retv > 0)
        {
            printf("Read byte: 0x%02X \n", buf);

            switch (state)
            {
            case START:
                if (buf == FLAG)
                    state = FLAG_RCV;
                break;

            case FLAG_RCV:
                state = START;
                if (buf == FLAG)
                    continue;
                if (buf == expectedAddr)
                    state = A_RCV;
                break;

            case A_RCV:
                state = START;
                if (buf == expectedCtrl)
                    state = C_RCV;
                else if (buf == FLAG)
                    state = FLAG_RCV;
                break;

            case C_RCV:
                state = START;
                if (buf == (expectedCtrl ^ expectedAddr))
                    state = BCC_OK;
                if (buf == FLAG)
                    state = FLAG_RCV;
                break;

            case BCC_OK:
                state = START;
                if (buf == FLAG)
                    state = STOP;
                break;

            default:
                state = START;
            }
        }

        if (state == STOP)
        {
            alarmDisable();
            return free(frame), free(frameString), 0;
        }

        if (alarmEnabled)
        {
            alarmEnabled = FALSE;

            if (alarmCount <= connectionParameters.nRetransmissions)
            {
                if (writeBytesSerialPort(frameString, BUF_SIZE) < 0)
                    return free(frame), free(frameString), -1;

                printf("Retransmiting frame! \n");
                alarm(connectionParameters.timeout);
            }

            state = START;
        }
    }

    alarmDisable();
    return free(frame), free(frameString), 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

int llopen(LinkLayer connection)
{
    memcpy(&connectionParameters, &connection, sizeof(connection));

    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
        return -1;

    switch (connectionParameters.role)
    {
    case LlTx:
        if (transmitFrame(ADDR_SEND, CTRL_SET, ADDR_SEND, CTRL_UA))
            return -1;
        printf("Transmiter Connected! \n");
        break;
    case LlRx:
        if (receiveFrame(ADDR_SEND, CTRL_SET))
            return -1;
        if (writeFrameToSerialPort(UA_Rx_Response, TRUE) < 0)
            return -1;
        printf("Receiver Connected! \n");
        break;
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *packet, int packetSize)
{
    if (packet == NULL)
        return -1;

    t_frame_ctrl ctrl = frameNumber ? CTRL_INFO1 : CTRL_INFO0;
    t_frame *frame = newFrame(ADDR_SEND, ctrl, (uint8_t *)packet, packetSize);
    if (frame == NULL)
        return -1;

    size_t stuffedSize = 0;
    uint8_t *stuffedString = frameToString(frame, &stuffedSize);

    t_state state = START;
    (void)signal(SIGALRM, alarmHandler);

    if (writeBytesSerialPort(stuffedString, stuffedSize) < 0)
        return free(stuffedString), free(frame), -1;

    alarm(connectionParameters.timeout);

    uint8_t receivedCtrl = 0;
    while (state != STOP && alarmCount <= connectionParameters.nRetransmissions)
    {
        uint8_t byte = 0;
        int retv = readByteSerialPort(&byte);

        if (retv < 0)
            return free(stuffedString), free(frame), -1;

        if (retv > 0)
        {
            switch (state)
            {
            case START:
                receivedCtrl = 0;
                if (byte == FLAG)
                    state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (byte == FLAG)
                    continue;
                state = START;
                if (byte == ADDR_SEND || byte == A_RCV)
                    state = A_RCV;
                break;
            case A_RCV:
                state = START;
                if (byte == CTRL_RR0 || byte == CTRL_RR1 || byte == CTRL_REJ0 || byte == CTRL_REJ1)
                {
                    state = C_RCV;
                    receivedCtrl = byte;
                }
                else if (byte == FLAG)
                    state = FLAG_RCV;
                break;
            case C_RCV:
                state = START;
                if (byte == (CTRL_RR0 ^ ADDR_SEND) || byte == (CTRL_RR1 ^ ADDR_SEND))
                    state = BCC_OK;
                else if (byte == FLAG)
                    state = FLAG_RCV;
                break;
            case BCC_OK:
                state = START;
                if (byte == FLAG)
                    state = STOP;
                break;
            default:
                state = START;
            }
        }

        if (state == STOP)
        {
            if (receivedCtrl == CTRL_REJ0 || receivedCtrl == CTRL_REJ1)
            {
                alarmEnabled = TRUE;
                alarmCount = 0;
                printf("Rejected, trying again...\n");
            }

            if (receivedCtrl == CTRL_RR0 || receivedCtrl == CTRL_RR1)
            {
                // TODO: Save time spent sending to stats

                alarmDisable();
                frameNumber = 1 - frameNumber;

                return free(stuffedString), free(frame), packetSize;
            }
        }

        if (alarmEnabled)
        {
            alarmEnabled = FALSE;

            if (alarmCount <= connectionParameters.nRetransmissions)
            {
                if (writeBytesSerialPort(stuffedString, stuffedSize) < 0)
                    return free(stuffedString), free(frame), -1;

                alarm(connectionParameters.timeout);
            }

            state = START;
        }
    }

    printf("llwrite() end!\n");

    alarmDisable();
    return free(stuffedString), free(frame), -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    if (packet == NULL)
    {
        printf("Packet in llread is null! \n");
        return -1;
    }

    t_state state = START;

    t_frame frame;
    frame.data = packet;
    frame.dataSize = MAX_PAYLOAD_SIZE;

    int idx = 0;
    while (state != STOP)
    {
        uint8_t buf = 0;
        int retv = readByteSerialPort(&buf);

        if (retv < 0)
        {
            printf("Error reading\n");
            return -1;
        }

        if (retv > 0)
        {
            switch (state)
            {
            case START:
                frame.c = 0;
                if (buf == FLAG)
                    state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (buf == FLAG)
                    continue;
                state = START;
                if (buf == ADDR_SEND)
                {
                    frame.a = buf;
                    state = A_RCV;
                }
                break;
            case A_RCV:
                state = START;
                if (buf == CTRL_INFO0 || buf == CTRL_INFO1)
                {
                    state = C_RCV;
                    frame.c = buf;
                }
                else if (buf == FLAG)
                    state = FLAG_RCV;
                break;
            case C_RCV:
                state = START;
                if (buf == (frame.c ^ ADDR_SEND))
                {
                    frame.bcc1 = frame.c ^ ADDR_SEND;
                    state = DATA_RCV;
                }
                else if (buf == FLAG)
                    state = FLAG_RCV;
                break;
            case DATA_RCV:
                if (buf == FLAG)
                {
                    frame.dataSize = idx;
                    printf("Finished frame reception\n");

                    if (frameDestuff(&frame))
                        return -1;

                    uint8_t bcc2 = 0x00;
                    for (size_t i = 0; i < frame.dataSize; i++)
                        bcc2 ^= frame.data[i];

                    t_frame *response;

                    if (bcc2 == frame.bcc2)
                    {
                        response = (frame.c == CTRL_INFO0) ? RR1_Command : RR0_Command;
                    }
                    else
                    {
                        if ((frameNumber == 0 && frame.c == CTRL_INFO1)
                            || (frameNumber == 1 && frame.c == CTRL_INFO0))
                        {
                            response = (frame.c == CTRL_INFO0) ? RR1_Command : RR0_Command;
                        }
                        else
                        {
                            response = (frame.c == CTRL_INFO0) ? REJ0_Command : REJ1_Command;
                        }
                    }

                    state = START;

                    if (writeFrameToSerialPort(response, FALSE) < 0)
                        return -1;

                    if (response->c == CTRL_REJ0 || response->c == CTRL_REJ1)
                    {

                        // TODO: Save error frame number to stats
                        
                        free(response);
                        break;
                    }

                    free(response);

                    if ((frameNumber == 0 && frame.c == CTRL_INFO0) || (frameNumber == 1 && frame.c == CTRL_INFO1))
                    {
                        frameNumber = 1 - frameNumber;

                        // TODO: Add bytes read and number of frames to stats

                        return frame.dataSize;
                    }

                    printf("Received duplicate frame\n");
                }
                else
                    packet[idx++] = buf;

                break;
            default:
                state = START;
            }
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    switch (connectionParameters.role)
    {
    case LlTx:
        if (transmitFrame(ADDR_SEND, CTRL_DISC, ADDR_RCV, CTRL_DISC))
            break;
        if (writeFrameToSerialPort(UA_Tx_Response, TRUE))
            break;
        printf("Disconnected Transmitter! \n");
        break;

    case LlRx:
        if (receiveFrame(ADDR_SEND, CTRL_DISC))
            break;
        if (transmitFrame(ADDR_RCV, CTRL_DISC, ADDR_RCV, CTRL_UA))
            break;
        printf("Disconnected Receiver! \n");
        break;
    }

    return closeSerialPort();
}
