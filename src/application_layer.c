// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "protocol.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct s_file {
  size_t  size;
  char    *name;
  size_t  receivedSize;
} t_file;

int statistics = FALSE;

int sendDataPacket(size_t dataSize, size_t sequenceNumber,  uint8_t *data) {
  if(data == NULL) return -1;

  uint8_t *packet = malloc(dataSize + 4);
  if(packet == NULL) return -1;

  packet[0] = DATA;
  packet[1] = sequenceNumber;
  packet[2] = dataSize >> 8;
  packet[3] = dataSize & 0xFF;
  memcpy(packet + 4, data, dataSize);

  int retv = llwrite(packet, dataSize + 4);
  return free(packet), retv;
}

int sendControlPacket(uint8_t controlField, const char *fileName, size_t fileSize) {
  if(fileName == NULL) return -1;

  uint8_t *v1 = ultoua(fileSize);
  if (v1 == NULL) return -1;

  uint8_t l1 = 0;
  for (; v1[l1]; l1++) ;

  uint8_t l2 = strlen(fileName);

  uint8_t *packet = calloc(1 + 2 + l1 + 2 + l2, sizeof(uint8_t));
  if (packet == NULL)
    return free(v1), -1;

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

uint8_t *parseDataPacket(uint8_t *packet, size_t expectedSequence, size_t *retSize)
{
  if (packet == NULL || packet[0] != DATA || retSize == NULL)
    return NULL;
  
  *retSize = (packet[2] << 8) + packet[3];

  if (expectedSequence != (size_t)packet[1])
    return NULL;

  return packet + 4;
}

int parseControlPacket(t_file *file, uint8_t *packet, int *isReceiving)
{
  if (file == NULL || packet == NULL || isReceiving == NULL)
    return -1;
  
  size_t i = 0;

  if (packet[i] != CTRL_START && packet[i] != CTRL_END)
    return -1;
  
  char *fileName = calloc(1000, sizeof(char));
  if (fileName == NULL)
    return -1;

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

  if (packet[0] == CTRL_START)
  {
    file->name = fileName;
    file->size = fileSize;
    file->receivedSize = 0;
    printf("Started reception of file '%s'\n", fileName);
  }

  if (packet[0] == CTRL_END)
  {
    if (file->size != file->receivedSize)
    {
      printf("Size at start and size at end differ (%ld vs %ld)\n",
        file->size, file->receivedSize);
      return free(fileName), -1;
    }
    if (strcmp(file->name, fileName) != 0)
    {
      printf("Name at start and name at end differ (%s vs %s)\n",
        file->name, fileName);
      return free(fileName), -1;
    }
    
    printf("Finished reception of file '%s'\n", fileName);
    free(fileName);
  }

  return 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {  
  LinkLayer connectionParameters;
  strcpy(connectionParameters.serialPort, serialPort);
  connectionParameters.role = strcmp(role, "tx") ? LlRx : LlTx;
  connectionParameters.baudRate = baudRate,
  connectionParameters.nRetransmissions = nTries;
  connectionParameters.timeout = timeout;
  
  t_file file;
  (void)file;

  if (llopen(connectionParameters) < 0) {
    printf("Error trying to start connection! \n");
    exit(-1);
  }

  if (llclose(statistics) < 0) {
    printf("Error trying to disconnect! \n");
    exit(-1);
  }
}
