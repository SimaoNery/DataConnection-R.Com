// Link layer protocol implementation

#include "link_layer.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "protocol.h"
#include "serial_port.h"
#include "utils.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

LinkLayer connectionParameters;
t_statistics stats = {0};
int frameNumber = 0;

// Alarm
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmCount++;
    alarmEnabled = TRUE;
    printf("[alarmHandler] Alarm %d\n", alarmCount);
}

void alarmDisable()
{
    alarm(0);
    alarmEnabled = FALSE;
    alarmCount = 0;
}

t_frame newFrame(t_frame_addr addr, t_frame_ctrl ctrl, uint8_t *data, size_t dataSize)
{
    if ((ctrl == CTRL_INFO0 || ctrl == CTRL_INFO1) && data == NULL)
        return info("newFrame", "INFO frames require data fields"), (t_frame){0};

    t_frame ret;

    ret.a = addr;
    ret.c = ctrl;
    ret.bcc1 = addr ^ ctrl;

    ret.bytesToStuff = 0;
    ret.bcc2 = 0x00;
    for (size_t i = 0; i < dataSize; i++)
    {
        if (data[i] == FLAG || data[i] == ESCAPE)
            ret.bytesToStuff += 1;
        ret.bcc2 ^= data[i];
    }

    if (ret.bcc2 == FLAG || ret.bcc2 == FLAG)
        ret.bytesToStuff += 1;

    ret.dataSize = dataSize;
    ret.data = data;

    return ret;
}

uint8_t *frameToString(t_frame *frame, size_t *finalSize)
{
    if (frame == NULL)
        return info("frameToString", "Can't convert NULL frame"), NULL;

    if (finalSize == NULL)
        return info("frameToString", "Can't save final size to NULL pointer"), NULL;

    int isInfoFrame = frame->c == CTRL_INFO0 || frame->c == CTRL_INFO1;

    uint8_t *ret = calloc(5 + isInfoFrame + frame->bytesToStuff + frame->dataSize, sizeof(uint8_t));
    if (ret == NULL)
        return info("frameToString", "Couldn't allocate memory for stuffed string"), NULL;

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

t_frame newSUFrame(t_frame_addr addr, t_frame_ctrl ctrl)
{
    return newFrame(addr, ctrl, NULL, 0);
}

int writeFrameToSerialPort(t_frame frame)
{
    size_t size = 0;
    uint8_t *string = frameToString(&frame, &size);
    if (string == NULL)
        return -1;

    int retv = writeBytesSerialPort(string, size);
    return free(string), retv;
}

int frameDestuff(t_frame *frame)
{
    if (frame == NULL)
        return err("frameDestuff", "Can't destuff NULL frame");

    if (frame->data == NULL)
        return err("frameDestuff", "Can't destuff NULL data");

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

int receiveFrame(t_frame expected)
{
    t_state state = START;

    while (state != STOP)
    {
        uint8_t buf = 0;
        int retv = readByteSerialPort(&buf);

        if (retv < 0)
            return spError("receiveFrame", TRUE);

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
                if (buf == expected.a)
                    state = A_RCV;
                else
                    state = START;
                break;

            case A_RCV:
                if (buf == expected.c)
                    state = C_RCV;
                else if (buf == FLAG)
                    state = FLAG_RCV;
                else
                    state = START;
                break;

            case C_RCV:
                if (buf == (expected.c ^ expected.a))
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

int transmitFrame(t_frame toSend, t_frame expected)
{
    t_state state = START;

    (void)signal(SIGALRM, alarmHandler);

    size_t size = 0;
    uint8_t *frameString = frameToString(&toSend, &size);
    if (frameString == NULL)
        return -1;
    if (writeBytesSerialPort(frameString, size) < 0)
        return free(frameString), spError("transmitFrame", FALSE);

    alarm(connectionParameters.timeout);

    while (state != STOP && alarmCount <= connectionParameters.nRetransmissions)
    {
        uint8_t buf = 0;
        int retv = readByteSerialPort(&buf);

        if (retv < 0)
            return spError("transmitFrame", TRUE);

        if (retv > 0)
        {
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
                if (buf == expected.a)
                    state = A_RCV;
                break;

            case A_RCV:
                state = START;
                if (buf == expected.c)
                    state = C_RCV;
                else if (buf == FLAG)
                    state = FLAG_RCV;
                break;

            case C_RCV:
                state = START;
                if (buf == (expected.c ^ expected.a))
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
            return free(frameString), 0;
        }

        if (alarmEnabled)
        {
            alarmEnabled = FALSE;

            if (alarmCount <= connectionParameters.nRetransmissions)
            {
                if (writeBytesSerialPort(frameString, BUF_SIZE) < 0)
                    return free(frameString), spError("transmitFrame", FALSE);

                info("transmitFrame", "Retransmiting frame!");
                alarm(connectionParameters.timeout);
            }

            state = START;
        }
    }

    alarmDisable();
    free(frameString);
    return err("transmitFrame", "Transmition failure - timeout");
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connection)
{
    gettimeofday(&stats.start, NULL);

    struct timeval start;

    memcpy(&connectionParameters, &connection, sizeof(connection));

    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
        return -1;

    switch (connectionParameters.role)
    {
    case LlTx:
        gettimeofday(&start, NULL);

        if (transmitFrame(SET_Command, UA_Rx_Response))
            return -1;

        stats.n_frames++;

        struct timeval end;
        gettimeofday(&end, NULL);

        stats.time_send_control += TIME_DIFF(start, end);

        info("llopen", "Transmiter Connected!");
        break;
    case LlRx:
        if (receiveFrame(SET_Command))
            return -1;

        stats.n_frames++;
        stats.bytes_read += BUF_SIZE;

        if (writeFrameToSerialPort(UA_Rx_Response) < 0)
            return spError("llopen", FALSE);
        info("llopen", "Receiver Connected!");
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
    t_frame frame = newFrame(ADDR_SEND, ctrl, (uint8_t *)packet, packetSize);

    size_t stuffedSize = 0;
    uint8_t *stuffedString = frameToString(&frame, &stuffedSize);

    t_state state = START;
    (void)signal(SIGALRM, alarmHandler);

    struct timeval start;
    gettimeofday(&start, NULL);

    if (writeBytesSerialPort(stuffedString, stuffedSize) < 0)
        return free(stuffedString), spError("llwrite", FALSE);

    alarm(connectionParameters.timeout);

    uint8_t receivedCtrl = 0;
    while (state != STOP && alarmCount <= connectionParameters.nRetransmissions)
    {
        uint8_t byte = 0;
        int retv = readByteSerialPort(&byte);

        if (retv < 0)
            return free(stuffedString), spError("llwrite", FALSE);

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
                if (byte == (CTRL_RR0 ^ ADDR_SEND) || byte == (CTRL_RR1 ^ ADDR_SEND) || byte == (CTRL_REJ0 ^ ADDR_SEND) || byte == (CTRL_REJ1 ^ ADDR_SEND))
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
                stats.n_errors++;
                info("llwrite", "Rejected, trying again...");
            }

            if (receivedCtrl == CTRL_RR0 || receivedCtrl == CTRL_RR1)
            {
                struct timeval end;
                gettimeofday(&end, NULL);
                stats.time_send_data += TIME_DIFF(start, end);

                alarmDisable();
                frameNumber = 1 - frameNumber;

                stats.n_frames++;

                return free(stuffedString), packetSize;
            }
        }

        if (alarmEnabled)
        {
            alarmEnabled = FALSE;

            if (alarmCount <= connectionParameters.nRetransmissions)
            {
                if (writeBytesSerialPort(stuffedString, stuffedSize) < 0)
                    return free(stuffedString), spError("llwrite", FALSE);

                alarm(connectionParameters.timeout);
            }

            state = START;
        }
    }

    alarmDisable();
    free(stuffedString);
    return err("llwrite", "Transmition failure - timeout");
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    if (packet == NULL)
        return err("llread", "Packet in llread is null!");

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
            return spError("llread", TRUE);

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

                    if (frameDestuff(&frame))
                        return err("llread", "Destuff failed");

                    uint8_t bcc2 = 0x00;
                    for (size_t i = 0; i < frame.dataSize; i++)
                        bcc2 ^= frame.data[i];

                    t_frame response;

                    if (bcc2 == frame.bcc2)
                    {
                        response = (frame.c == CTRL_INFO0) ? RR1_Command : RR0_Command;
                    }
                    else
                    {
                        response = (frame.c == CTRL_INFO0) ? REJ0_Command : REJ1_Command;
                    }

                    state = START;
                    idx = 0;

                    if (writeFrameToSerialPort(response) < 0)
                        return spError("llread", FALSE);

                    if (response.c == CTRL_REJ0 || response.c == CTRL_REJ1)
                    {
                        info("llread", "Invalid frame, trying again...");
                        stats.n_errors++;
                        continue;
                    }

                    if ((frameNumber == 0 && frame.c == CTRL_INFO0) || (frameNumber == 1 && frame.c == CTRL_INFO1))
                    {
                        frameNumber = 1 - frameNumber;

                        stats.bytes_read += frame.dataSize + 6;
                        stats.n_frames++;

                        return frame.dataSize;
                    }

                    info("llread", "Received duplicate frame");
                    continue;
                }

                if (idx > MAX_PAYLOAD_SIZE)
                {
                    info("llread", "Payload is too big! Returning to start");
                    state = START;
                    idx = 0;
                    continue;
                }

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
    struct timeval start;

    switch (connectionParameters.role)
    {
    case LlTx:
        gettimeofday(&start, NULL);

        if (transmitFrame(DISC_Tx_Command, DISC_Rx_Command))
            break;

        struct timeval end;
        gettimeofday(&end, NULL);

        stats.n_frames++;

        if (writeFrameToSerialPort(UA_Tx_Response) < 0)
        {
            spError("llclose", FALSE);
            break;
        }

        stats.time_send_control += TIME_DIFF(start, end);
        stats.n_frames++;

        info("llclose", "Disconnected Transmitter!");
        break;

    case LlRx:
        if (receiveFrame(DISC_Tx_Command))
            break;

        stats.n_frames++;
        stats.bytes_read += BUF_SIZE;

        if (transmitFrame(DISC_Rx_Command, UA_Tx_Response))
            break;

        stats.n_frames++;
        stats.bytes_read += BUF_SIZE;

        info("llclose", "Disconnected Receiver!");
        break;
    }

    printf("\n");

    if (showStatistics)
    {
        if (connectionParameters.role == LlRx)
        {
            struct timeval end;
            gettimeofday(&end, NULL);

            time_t totalTime = TIME_DIFF(stats.start, end);
            printf("Showing link-layer protocol statistics\n"
                   "  - Frames:\n"
                   "    • Number of (unstuffed) bytes received: %ld\n"
                   "    • Number of accepted frames: %ld\n"
                   "    • Number of error frames: %ld\n"
                   "    • Average size of frame: %ld\n"
                   "  - Efficiency:\n"
                   "    • Reception velocity (bits/s): %.2f\n"
                   "    • Overall time taken: %ld seconds\n",
                   stats.bytes_read,
                   stats.n_frames,
                   stats.n_errors,
                   stats.bytes_read / stats.n_frames,
                   stats.bytes_read * 8.0 / totalTime,
                   totalTime);
        }
        else
        {
            printf("Showing link-layer protocol statistics\n"
                   "  - Frames:\n"
                   "    • Number of (unstuffed) bytes received: %ld\n"
                   "    • Number of accepted frames: %ld\n"
                   "    • Number of error frames: %ld\n"
                   "    • Average size of frame: %ld\n"
                   "  - Efficiency:\n"
                   "    • Total time taken while sending and receving control frames: %f seconds\n"
                   "    • Total time taken while sending and receving data frames: %f seconds\n"
                   "    • Average time taken to send a frame: %f seconds\n",
                   stats.bytes_read,
                   stats.n_frames,
                   stats.n_errors,
                   stats.bytes_read / stats.n_frames,
                   stats.time_send_control,
                   stats.time_send_data,
                   (stats.time_send_data + stats.time_send_control) / stats.n_frames);
        }
    }

    return closeSerialPort();
}
