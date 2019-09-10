#ifndef _GPS
#define _GPS

#include <sys/time.h>

#ifdef __cplusplus
extern "C" 
{
#endif /* __cplusplus */

#ifndef NULL
#define NULL 0
#endif // !NULL

typedef const void* GPSHandle;

typedef struct GPSPosition
{
    double          x;          // longitude
    double          y;          // latitude
    double          z;          // altitude
    struct timeval  gps_time;   // gps time of GPS position fix
    struct timeval  sys_time;   // system time of GPS position fix
    int             xyvalid;
    int             zvalid;
    int             tvalid;     // true if time _and_ date was given in NMEA
    int			    stale;
} GPSPosition;


char* GPSMemoryInit(const char* keyFile, unsigned int size);

inline GPSHandle GPSPublishInit(const char* keyFile)
    {return (GPSHandle)GPSMemoryInit(keyFile, sizeof(GPSPosition));}
void GPSPublishUpdate(GPSHandle gpsHandle, const GPSPosition* currentPosition);
void GPSPublishShutdown(GPSHandle gpsHandle, const char* keyFile);

GPSHandle GPSSubscribe(const char* keyFile);
void GPSGetCurrentPosition(GPSHandle gpsHandle, GPSPosition* currentPosition);
void GPSUnsubscribe(GPSHandle gpsHandle);

// Generic data publishing
unsigned int GPSSetMemory(GPSHandle gpsHandle, unsigned int offset, 
                          const char* buffer, unsigned int len);
unsigned int GPSGetMemorySize(GPSHandle gpsHandle);
unsigned int GPSGetMemory(GPSHandle gpsHandle, unsigned int offset, 
                          char* buffer, unsigned int len);


#ifdef __cplusplus
}
#endif /* __cplusplus */ 
    
#endif // _GPS
