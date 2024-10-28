// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CTRL_START 1
#define DATA 2
#define CTRL_END 3

#define TYPE_FSIZE 0
#define TYPE_FNAME 1

typedef struct s_file {
  size_t size;
  char *name;
  size_t receivedSize;
} t_file;

int sendDataPacket(size_t dataSize, size_t sequenceNumber, uint8_t *data) {
  if (data == NULL)
    return -1;

  uint8_t *packet = malloc(dataSize + 4);
  if (packet == NULL)
    return -1;

  packet[0] = DATA;
  packet[1] = sequenceNumber;
  packet[2] = dataSize >> 8;
  packet[3] = dataSize & 0xFF;
  memcpy(packet + 4, data, dataSize);

  int retv = llwrite(packet, dataSize + 4);
  return free(packet), retv;
}

int sendControlPacket(uint8_t controlField, const char *fileName, size_t fileSize) {
  if (fileName == NULL) {
    printf("File name is null! \n");
    return -1;
  }

  uint8_t *v1 = ultoua(fileSize);
  if (v1 == NULL)
    return -1;

  uint8_t l1 = 0;
  for (; v1[l1]; l1++);

  uint8_t l2 = strlen(fileName);

  uint8_t *packet = calloc(1 + 2 + l1 + 2 + l2, sizeof(uint8_t));
  if (packet == NULL) {
    printf("Packet is null! \n");
    return free(v1), -1;
  }

  size_t i = 0;
  packet[i++] = controlField;
  packet[i++] = TYPE_FSIZE;
  packet[i++] = l1;
  memcpy(packet + i, v1, l1);
  i += l1;

  packet[i++] = TYPE_FNAME;
  packet[i++] = l2;
  memcpy(packet + i, fileName, l2);
  i += l2;

  int retv = llwrite(packet, i);
  return free(packet), free(v1), retv;
}

uint8_t *parseDataPacket(uint8_t *packet, size_t expectedSequence, size_t *retSize) {
  if (packet == NULL || packet[0] != DATA || retSize == NULL)
    return NULL;

  *retSize = (packet[2] << 8) + packet[3];

  if (expectedSequence != (size_t) packet[1])
    return NULL;

  return packet + 4;
}

int parseControlPacket(t_file *file, uint8_t *packet, int *isReceiving) {
  if (file == NULL || packet == NULL || isReceiving == NULL) {
    printf("Something is wrong with parse control packet! \n");
    return -1;
  }

  size_t i = 0;

  if (packet[i] != CTRL_START && packet[i] != CTRL_END) {
    printf("Control field mismatch: expected %d or %d, got %d\n", CTRL_START, CTRL_END, packet[i]);
    return -1;
  }

  char *fileName = calloc(1000, sizeof(char));
  if (fileName == NULL) {
    printf("Coulnt allocate memory for filename! \n");
    return -1;
  }

  *isReceiving = packet[i++] == CTRL_START;

  if (packet[i++] != TYPE_FSIZE)
    return free(fileName), -1;

  uint8_t l1 = packet[i++];
  uint8_t *v1 = calloc(l1, sizeof(uint8_t));
  if (v1 == NULL)
    return free(fileName), -1;
  memcpy(v1, packet + i, l1);
  i += l1;

  size_t fileSize = uatoi(v1, l1);

  if (packet[i++] != TYPE_FNAME)
    return free(fileName), free(v1), -1;
  uint8_t l2 = packet[i++];
  memcpy(fileName, packet + i, l2);

  if (packet[0] == CTRL_START) {
    file->name = fileName;
    file->size = fileSize;
    file->receivedSize = 0;
    printf("Started reception of file '%s', File Size: %ld\n", fileName, fileSize);
  }

  if (packet[0] == CTRL_END) {
    if (file->size != file->receivedSize) {
      printf("Size at start and size at end differ (%ld vs %ld)\n",
             file->size, file->receivedSize);
      return free(fileName), free(v1), -1;
    }
    if (strcmp(file->name, fileName) != 0) {
      printf("Name at start and name at end differ (%s vs %s)\n",
             file->name, fileName);
      return free(fileName), free(v1), -1;
    }

    printf("Finished reception of file '%s'\n", fileName);
    free(fileName);
  }

  return free(v1), 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
  if (serialPort == NULL || role == NULL || filename == NULL) {
    printf("Error trying to initialize the application layer protocol! \n");
    exit(-1);
  }

  LinkLayer connectionParameters;
  strcpy(connectionParameters.serialPort, serialPort);
  connectionParameters.role = strcmp(role, "tx") ? LlRx : LlTx;
  connectionParameters.baudRate = baudRate,
  connectionParameters.nRetransmissions = nTries;
  connectionParameters.timeout = timeout;

  size_t bytes = 0;
  t_file fileInfo;
  FILE *file;
  uint8_t *buffer;

  if (llopen(connectionParameters) < 0) {
    printf("Error trying to start connection! \n");
    llclose(FALSE);
    return;
  }

  printf("\n General Connection Was Established! \nStarting data sharing! \n \n ");

  switch (connectionParameters.role) {
    case LlTx:
      file = fopen(filename, "rb");
      if (file == NULL) {
        printf("Couldn't find the file! \n");
        llclose(FALSE);
        return;
      }
      fseek(file, 0, SEEK_END);
      size_t fileSize = ftell(file);
      fseek(file, 0, SEEK_SET);

      if (sendControlPacket(CTRL_START, filename, fileSize) < 0) {
        printf("Couln't send control packet! \n");
        fclose(file);
        llclose(FALSE);
        return;
      }

      printf("Sent START control packet! \n");

      bytes = 0;
      buffer = malloc(MAX_PAYLOAD_SIZE + 20);
      if (buffer == NULL) {
        printf("Couldn't allocate buffer memory! \n");
        llclose(FALSE);
        return;
      }

      size_t sequenceNumber = 0;
      while ((bytes = fread(buffer, 1, MAX_PAYLOAD_SIZE, file)) > 0) {
        size_t sendedData = sendDataPacket(bytes, sequenceNumber, buffer);
        if (sendedData < 0) {
          printf("Error sending data packet! \n");
          free(buffer);
          fclose(file);
          llclose(FALSE);
          return;
        }

        printf("Sent packet %ld \n", sequenceNumber);
        sequenceNumber = sequenceNumber >= 99 ? 0 : sequenceNumber + 1;
      }

      printf("All data has been sent! \n");

      if (sendControlPacket(CTRL_END, filename, fileSize) < 0) {
        printf("Error sending end control packet! \n");
        fclose(file);
        free(buffer);
        llclose(FALSE);
        return;
      }

      printf("Sent END control packet! \n");

      fclose(file);
      free(buffer);
      break;

    case LlRx:
      file = fopen(filename, "wb");
      if (file == NULL) {
        printf("Couldn't find the file! \n");
        llclose(FALSE);
        return;
      }

      buffer = malloc(MAX_PAYLOAD_SIZE + 20);
      if (buffer == NULL) {
        printf("Couldn't allocate buffer memory! \n");
        llclose(FALSE);
        return;
      }

      uint8_t *packet = malloc(MAX_PAYLOAD_SIZE + 20);
      if (packet == NULL) {
        printf("Couldn't allocate packet memory! \n");
        free(buffer);
        llclose(FALSE);
        return;
      }

      bytes = 0;
      int isReceiving = TRUE;
      int expectedNumber = 0;

      while (isReceiving) {
        bytes = llread(buffer);
        if (bytes <= 0) {
          printf("Failed to read! \n");
          fclose(file);
          free(buffer);
          llclose(FALSE);
          return;
        }

        if (bytes == 0)
            continue;

        printf("buffer[0] value: %d\n", buffer[0]);

        if (buffer[0] == CTRL_START || buffer[0] == CTRL_END) {
          if (parseControlPacket(&fileInfo, buffer, &isReceiving) < 0) {
            printf("Error parsing control packet! \n");

            fclose(file);
            free(buffer);
            free(packet);
            llclose(FALSE);
            return;
          }

          printf("Parsed a control packet! \n");
        }

        if (buffer[0] == DATA) {
          uint8_t *receivedData = parseDataPacket(buffer, expectedNumber, &bytes);
          if (receivedData == NULL) {
            printf("Error parsing data packet! \n");

            fclose(file);
            free(buffer);
            free(packet);
            llclose(FALSE);
            return;
          }

          fwrite(receivedData, sizeof(uint8_t), bytes, file);
          fileInfo.receivedSize += bytes;

          expectedNumber = expectedNumber >= 99 ? 0 : expectedNumber + 1;
        }
      }

      printf("All data has been received! \n");

      fclose(file);
      free(buffer);
      free(packet);
      break;

    default:
      break;
  }

  if (llclose(TRUE) < 0) {
    printf("Error trying to disconnect! \n");
    exit(-1);
  }
}
