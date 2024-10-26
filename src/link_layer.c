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

LinkLayer connectionParameters;
int frameNumber = 0;

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

  printf("\n Waiting for retv to read... \n");

  while (state != STOP) {
    uint8_t buf = 0;
    int retv = readByteSerialPort(&buf);

    if (retv < 0) {
      printf("Error trying to read frame! \n");
      return -1;
    }

    if (retv > 0) {
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

  (void) signal(SIGALRM, alarmHandler);

  if (writeBytesSerialPort(frame_buffers[frameType], BUF_SIZE) < 0)
    return -1;

  printf("Wrote frame! \n");

  alarm(connectionParameters.timeout);

  while (state != STOP && alarmCount <= connectionParameters.nRetransmissions) {
    uint8_t buf = 0;
    int retv = readByteSerialPort(&buf);

    if (retv < 0) {
      printf("Error trying to read frame! \n");
      return -1;
    }

    if (retv > 0) {
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

uint8_t *byteStuffing(const uint8_t *buf, int bufSize, int *stuffedSize) {
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

uint8_t *assembleFrame(const uint8_t *stuffedPacket, int stuffedSize, const unsigned char *packet, int packetSize) {
  uint8_t *frame = malloc(stuffedSize + 7 * sizeof(uint8_t));
  if (frame == NULL) return NULL;

  frame[0] = FLAG;
  frame[1] = ADDR_SEND;
  frame[2] = frameNumber ? CTRL_INFO0 : CTRL_INFO1;
  frame[3] = frame[1] ^ frame[2];

  memcpy(frame + 4, stuffedPacket, stuffedSize);

  uint8_t bcc = 0;
  for (int i = 0; i < packetSize; i++)
    bcc ^= packet[i];

  if (bcc == FLAG || bcc == ESCAPE) {
    frame[4 + stuffedSize] = ESCAPE;
    frame[5 + stuffedSize] = bcc ^ ESCAPE_OFFSET;
    stuffedSize++;
  }
  else {
    frame[4 + stuffedSize] = bcc;
  }

  frame[5 + stuffedSize] = FLAG;

  return frame;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *packet, int packetSize) {
  if (packet == NULL)
    return -1;

  int stuffedSize = 0;
  uint8_t *stuffedPacket = byteStuffing(packet, packetSize, &stuffedSize);
  if (stuffedPacket == NULL)
    return -1;
  
  uint8_t *frame = assembleFrame(stuffedPacket, stuffedSize, packet, packetSize);
  if(frame == NULL) return free(stuffedPacket), -1;

  t_state state = START;
  (void) signal(SIGALRM, alarmHandler);

  if (writeBytesSerialPort(frame, stuffedSize + 6) < 0)
    return free(stuffedPacket), free(frame), -1;

  alarm(connectionParameters.timeout);

  uint8_t receivedAddr = 0, receivedCtrl = 0;
  while (state != STOP && alarmCount <= connectionParameters.timeout) {
    uint8_t buf = 0;
    int retv = readByteSerialPort(&buf);

    if (retv < 0)
      return free(stuffedPacket), free(frame), -1;

    if (retv > 0) {
      switch (state) {
        case START:
          receivedAddr = 0;
          receivedCtrl = 0;
          if (buf == FLAG)
            state = FLAG_RCV;
          break;
        case FLAG_RCV:
          if (buf == FLAG)
            continue;
          if (buf == ADDR_SEND || buf == A_RCV) {
            state = A_RCV;
            receivedAddr = buf;
          }
          else
            state = START;
          break;
        case A_RCV:
          if (buf == CTRL_RR0 || buf == CTRL_RR1 || buf == CTRL_REJ0 || buf == CTRL_REJ1) {
            state = C_RCV;
            receivedCtrl = buf;
          }
          else if (buf == FLAG)
            state = FLAG_RCV;
          else
            state = START;
          break;
        case C_RCV:
          if (buf == (CTRL_RR0 ^ ADDR_SEND) || buf == (CTRL_RR1 ^ ADDR_SEND))
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
      if (receivedAddr == CTRL_REJ0 || receivedCtrl == CTRL_REJ1) {
        alarmEnabled = TRUE;
        alarmCount = 0;
        printf("Rejected, trying again...");
      }

      if (receivedCtrl == CTRL_RR0 || receivedCtrl == CTRL_RR1) {

        // TODO: Save time spent sending to stats

        alarmDisable();
        frameNumber = 1 - frameNumber;

        return free(stuffedPacket), free(frame), packetSize;
      }
    }

    if (alarmEnabled) {
      alarmEnabled = FALSE;

      if (alarmCount <= connectionParameters.nRetransmissions) {
        if (writeBytesSerialPort(frame, stuffedSize + 6) < 0)
          return free(stuffedPacket), free(frame), -1;

        alarm(connectionParameters.timeout);
      }

      state = START;
    }
  }

  alarmDisable();
  return free(stuffedPacket), free(frame), -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  if (packet == NULL)
    return -1;

  t_state state = START;

  uint8_t receivedCtrl = 0;
  int idx = 0;

  while (state != STOP) {
    uint8_t buf = 0;
    int retv = readByteSerialPort(&buf);

    if (retv < 0) {
      printf("Error reading\n");
      return -1;
    }

    if (retv > 0) {
      switch (state) {
        case START:
          receivedCtrl = 0;
          if (buf == FLAG)
            state = FLAG_RCV;
          break;
        case FLAG_RCV:
          if (buf == FLAG)
            continue;
          if (buf == ADDR_SEND)
            state = A_RCV;
          else
            state = START;
          break;
        case A_RCV:
          if (buf == CTRL_INFO0 || buf == CTRL_INFO1) {
            state = C_RCV;
            receivedCtrl = retv;
          }
          else if (buf == FLAG)
            state = FLAG_RCV;
          else
            state = START;
          break;
        case C_RCV:
          if (buf == (receivedCtrl ^ ADDR_SEND))
            state = DATA_RCV;
          else {
            if (buf == FLAG)
              state = FLAG_RCV;
            else
              state = START;
          }
          break;
        case DATA_RCV:
          if (buf == FLAG) {
            int newSize = 0;

            uint8_t *destuffedPacket = byteDestuffing(packet, idx, &newSize);
            if (destuffedPacket == NULL)
              return -1;

            uint8_t receivedBCC2 = destuffedPacket[newSize - 1];

            uint8_t bcc2 = 0x00;
            for (size_t i = 0; i < newSize; i++) bcc2 ^= destuffedPacket[i];

            uint8_t *response;

            if (bcc2 == receivedBCC2) {
              response = (receivedCtrl == CTRL_INFO0) ? RR1_Command : RR0_Command;
            }
            else {
              if ((frameNumber == 0 && receivedCtrl == CTRL_INFO1) || (frameNumber == 1 && receivedCtrl == CTRL_INFO0)) {
                response = (receivedCtrl == CTRL_INFO0) ? RR1_Command : RR0_Command;
              }
              else {
                response = (receivedCtrl == CTRL_INFO0) ? REJ0_Command : REJ1_Command;
              }
            }

            state = START;

            if (writeBytesSerialPort(response, BUF_SIZE) < 0)
              return -1;

            if (response == REJ0_Command || response == REJ1_Command) {

              // TODO: Save error frame number to stats

              free(destuffedPacket);
              break;
            }

            if ((frameNumber == 0 && receivedCtrl == CTRL_INFO0) || (frameNumber == 1 && receivedCtrl == CTRL_INFO1)) {
              frameNumber = 1 - frameNumber;
              printf("Bytes received: %d\n", newSize);

              // TODO: Add bytes read and number of frames to stats

              free(packet);
              packet = destuffedPacket;
              return newSize;
            }

            printf("Received duplicate\n");
            free(destuffedPacket);
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
