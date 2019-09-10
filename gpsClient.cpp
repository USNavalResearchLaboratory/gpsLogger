// Example GPSSubscribe client

#include "gpsPub.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    GPSHandle gpsHandle = GPSSubscribe();
    if (!gpsHandle)
    {
        fprintf(stderr, "gpsClient: Error subscribing to GPS position report.\n");
        exit(-1);   
    }
    while(1)
    {
        GPSPosition p;
        GPSGetCurrentPosition(gpsHandle, &p);
        fprintf(stdout, "currentPosition: %f:%f:%f\n",
                p.x, p.y, p.z);
        sleep(1);   
    }
    GPSUnsubscribe(gpsHandle);
}  // end main()
