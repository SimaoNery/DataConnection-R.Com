// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int statistics = FALSE;

unsigned char *packet;
int packetSize;

int sendDataPacket(size_t dataSize, size_t sequenceNumber,  uint8_t *data) {
  if(data == NULL) return -1;

  uint8_t *packet = malloc(dataSize + 4);
  if(packet == NULL) return -1;

  packet[0] = DATA;
  packet[1] = sequenceNumber;
  packet[2] = dataSize >> 8;
  packet[3] = dataSize & 0xFF;
  memcpy(packet + 4, data, dataSize);

  int res = llwrite(packet, dataSize + 4);
  free(packet);

  return res;
}

int sendControlPacket(uint8_t controlField, const char *fileName, size_t fileSize) {
  if(fileName == NULL) return -1;

  uint8_t *v1 = itoa();
  uint8_t l1 = sizeof(v1);
  if(v1 == NULL) return -1;

  uint8_t l2 = strlen(fileName);

}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {  
  LinkLayer connectionParameters;
  strcpy(connectionParameters.serialPort, serialPort);
  connectionParameters.role = strcmp(role, "tx") ? LlRx : LlTx;
  connectionParameters.baudRate = baudRate,
  connectionParameters.nRetransmissions = nTries;
  connectionParameters.timeout = timeout;

  if (llopen(connectionParameters) < 0) {
    printf("Error trying to start connection! \n");
    exit(-1);
  }

  if (llwrite(packet, packetSize) < 0) {
    printf("Error writting data! \n");
    exit(-1);
  }

  if (llread(packet) < 0) {
    printf("Error trying to read data! \n");
    exit(-1);
  }

  if (llclose(statistics) < 0) {
    printf("Error trying to disconnect! \n");
    exit(-1);
  }
}
