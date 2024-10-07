#include "alarm.h"

#define _POSIX_SOURCE 1 // POSIX compliant source

int alarmEnabled = FALSE;
int alarmCount = -1;
void (*alarmCallout)(void) = NULL;

void setAlarm(int timeout, int retryNum, void (*callout)(void))
{
    if (timeout < 1 || retryNum < 0 || callout == NULL)
        return;
    
    alarmEnabled = TRUE;
    alarmCount = retryNum;
    alarmCallout = callout;

    (void)signal(SIGALRM, alarmCallout);

    while (alarmCount)
    {
        if (alarmEnabled == FALSE)
            break;

        alarm(timeout);
    }
}
