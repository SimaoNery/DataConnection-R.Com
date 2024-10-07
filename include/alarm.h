#ifndef ALARM_H
#define ALARM_H

#include <signal.h>
#include <stdlib.h>
#include "link_layer.h"

void setAlarm(int timeout, int retryNum, void (*callout)(void));

#endif