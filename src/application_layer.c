// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int statistics = FALSE;

unsigned char *packet;
int packetSize;

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
