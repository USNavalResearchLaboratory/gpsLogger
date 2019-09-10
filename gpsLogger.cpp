
#include "gpsPub.h"
#include "nmeaParse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>  // for process priority boost

#define LINUX 1

#define VERSION "1.8"

#ifdef LINUX
#include <linux/serial.h>  // for Linux low latency option
#endif // LINUX

bool GPSGetTimeAndPosition(char* lineBuffer, struct timeval* currentTime,
                           double* xPos, double* yPos, double* zPos);

class GPSLogger
{
    public:
        GPSLogger();
        bool Main(int argc, char* argv[]);
        void SetStale();
        void Cleanup();
        void Stop();
        
    private:
        bool        running;
        FILE*       log_ptr;
        int         serial_fd;
        GPSHandle   gps_handle;
        GPSPosition p;
            
        static void SignalHandler(int sigNum);
        static void Usage();
};  // end class GPSLogger

GPSLogger theApp;
int main(int argc, char* argv[])
{
    if (theApp.Main(argc, argv))
    {
        fprintf(stderr, "gpsLogger: Done.\n");
        exit(0);
    }
    else
    {
        fprintf(stderr, "gpsLogger: Unexpected finish!\n");
        exit(-1);
    }
}  // end main()


GPSLogger::GPSLogger()
    : running(false), log_ptr(NULL), serial_fd(-1), gps_handle(NULL)
{
}

bool GPSLogger::Main(int argc, char* argv[])
{
    bool setTime = false;
    bool logging = true;
    const char* serialDevice = "/dev/ttyS0";
    const char* pubFile = NULL;
    const char* logFileName = NULL;
    unsigned int baud = 4800;
    bool nmeaParse = true;  // NMEA parse by default
    bool use_pps = false;
    bool configureGPS35 = false;
    bool debug = false;
    bool requireChecksum = false;
    bool largeTimeChangeFlag = false;
    unsigned long largeTimeChangeDelta = 0;
    bool forceClock = false;  // if true, force clock using settimeofday()
                              // instead of adjtime() on first sync
    
    // 1) Parse command-line options
    char** ptr = argv + 1;
    while (*ptr)
    {
        if (!strncmp("set", *ptr, 3))
        {
            ptr++;
            setTime = true;
        }
        else if (!strcmp("pps", *ptr))
        {
            ptr++;
            use_pps = true;
        }
        else if (!strncmp("gps35", *ptr, 3))
        {
            ptr++;
            configureGPS35 = true;
        }
        else if (!strncmp("ver", *ptr, 3))
        {
            ptr++;
            fprintf(stderr, "gpsLogger Version %s\n", VERSION);
        }
        else if (!strncmp("bin", *ptr, 3))
        {
            ptr++;
            nmeaParse = false;
        }
        else if (!strcmp("noLog", *ptr))
        {
            ptr++;
            logging = false;
        }
        else if (!strcmp("debug", *ptr))
        {
            ptr++;
            debug = true;
        }
        else if (!strncmp("check", *ptr, 5))
        {
            ptr++;
            requireChecksum = true;
        }
        else if (!strncmp("force", *ptr, 5))
        {
            ptr++;
            forceClock = true;
        }
        else if (!strncmp("log", *ptr, 3))
        {
            ptr++;
            if (*ptr)
            {
                logFileName = *ptr++;
            }
            else
            {
                fprintf(stderr, "gpsLogger: No <logFile> argument given!\n");
                Usage();
                return false;   
            }
        }
        else if (!strncmp("dev", *ptr, 3))
        {
            ptr++;
            if (*ptr)
            {
                serialDevice = *ptr++;
            }
            else
            {
                fprintf(stderr, "gpsLogger: No <serialDevice> argument given!\n");
                Usage();
                return false;   
            }
        }
        else if (!strcmp("speed", *ptr))
        {
            ptr++;
            if (*ptr)
            {
                baud = atoi(*ptr++);
            }
            else
            {
                fprintf(stderr, "gpsLogger: No <baudRate> argument given!\n");
                Usage();
                return false;   
            }
        }
        else if (!strncmp("pub", *ptr, 3))
        {
            ptr++;
            if (*ptr)
            {
                pubFile = *ptr++;
            }
            else
            {
                fprintf(stderr, "gpsLogger: No <pubFile> argument given!\n");
                Usage();
                return false;   
            }
        }
        else
        {
            fprintf(stderr, "gpsLogger: Invalid command!\n");
            Usage();
            return false;
        }
    }  // end while(*ptr)
    
#ifdef LINUX
    // Boost process priority for real-time operation
    // (This _may_ work on Linux-only at this point)
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority =  sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &schp))
    {
        schp.sched_priority =  sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &schp))
        {
            perror("gpsLogger: Warning! Couldn't set any real-time priority");
        }   
    }
#endif // LINUX
    
    // 2) Open log file (if applicable)
    if (logging)
    {
        if (logFileName)
        {
            if (!(log_ptr = fopen(logFileName, "w+")))
            {
                perror("gpsLogger: Error opening <logFile>!");
                return false;
            }   
        }
        else
        {
            log_ptr = stdout;
        }    
    }    
    
    // 3) Init GPS shared memory publishing
    if (!(gps_handle = GPSPublishInit(pubFile)))
    {
        fprintf(stderr, "gpsLogger: Error creating shared memory!\n");
        Cleanup();
        return false;   
    }
    
    // 4) Open up serial port for reading
    int flags;
    if (configureGPS35)
        flags = O_RDWR;
    else
        flags = O_RDONLY;
    int serial_fd = open(serialDevice, flags);
    if (serial_fd < 0)
    {
        perror("gpsLogger: Error opening serial port!");
        Cleanup();
        return false;   
    }
    // Set up serial port attributes
    struct termios attr;
    if (tcgetattr(serial_fd, &attr) < 0)
    {
        perror("gpsLogger: Error getting serial port settings!");
        close(serial_fd);
        Cleanup();
        return false;   
    }
    attr.c_cflag &= ~PARENB;  // no parity
    attr.c_cflag &= ~CSIZE;   // 8-bit bytes (first, clear mask, 
    attr.c_cflag |= CS8;      //              then, set value)
    cfmakeraw(&attr);
    attr.c_cflag |= CLOCAL;
    
    speed_t speed;
    switch(baud)
    {
        case 4800:
            speed = B4800;
            break;
        case 9600:
            speed = B9600;
            break;
            
        default:
            fprintf(stderr, "gpsLogger: Invalid <baudRate> setting!\n");
            Cleanup();
            return false;
    }
    
    if (cfsetispeed(&attr, speed))
    {
        perror("gpsLogger: cfsetispeed() error");
        close(serial_fd);
        Cleanup();
        return false;   
    }
    if (cfsetospeed(&attr, speed))
    {
        perror("gpsLogger: cfsetospeed() error");
        close(serial_fd);
        Cleanup();
        return false;   
    }
    
    attr.c_cc[VTIME]    = 100; // (100 * 0.1 sec) 10 second timeout
    attr.c_cc[VMIN]     = 0;   // 1 char satisfies read
    
    if (tcsetattr(serial_fd, TCSANOW, &attr) < 0)
    {
        perror("gpsLogger: Error setting serial port settings");
        close(serial_fd); 
        Cleanup();
        return false;   
    }

#ifdef ASYNC_LOW_LATENCY  // (LINUX only?)  
    // Try to set low latency
    struct serial_struct serinfo;
    if (ioctl(serial_fd, TIOCGSERIAL, &serinfo) < 0)
    {
		perror("gpsLogger: Cannot get serial info");
        close(serial_fd);
		Cleanup();
        return false;
    }
    else
    {
        serinfo.flags |= ASYNC_LOW_LATENCY;
        if (ioctl(serial_fd, TIOCSSERIAL, &serinfo) < 0) 
        {
		    perror("gpsLogger: Cannot set low latency option");
            close(serial_fd);
		    Cleanup();
            return false;
	    }
    }
#endif // ASYNC_LOW_LATENCY
    
    if (configureGPS35)
    {
        fprintf(stderr, "gpsLogger: configuring GPS device for PPS operation ...\n");
        // First, query configuration
        const char* cmd = "$PGRMCE\r\n";
        unsigned int len = strlen(cmd);
        unsigned int put = 0;
        while (put < len)
        {
            int result = write(serial_fd, cmd+put, len-put);
            if (result < 0)
            {
                if (EINTR != errno)
                {
                    perror("gpsLogger: GPS device serial port write() error");
                    break;
                }
            }
            else
            {
                put += result;    
            }            
        }
        // Write/verify Garmin GPS-35 PPS enabled config   
        cmd = "$PGRMC,,,,,,,,,,,,2,5,10\r\n";
        //const char* cmd = "$PGRMCE\r\n";
        len = strlen(cmd);
        put = 0;
        while (put < len)
        {
            int result = write(serial_fd, cmd+put, len-put);
            if (result < 0)
            {
                if (EINTR != errno)
                {
                    perror("gpsLogger: GPS device serial port write() error");
                    break;
                }
            }
            else
            {
                put += result;    
            }            
        }
        if (put >= len)
        {
            char buf[512];
            for (int k = 0; k < 5; k++)
            {
                buf[0] = '\0';
                unsigned int i = 0;
                while (i < 511)
                {
                  int result = read(serial_fd, &buf[i], 1);
                  if (result < 0)
                  {
                      if (EINTR != errno)
                      {
                           perror("gpsLogger: Serial port read() error");
                           k = 5; 
                           break;   
                      }
                  }
                  else if (result == 0)
                  {
                      fprintf(stderr, "gpsLogger: Serial port read() timed out!\n");
                      k = 5;
                      break;
                  }
                  else
                  {
                      if ('\n' == buf[i]) 
                          break;
                      else
                          i++;
                  }
                }
                buf[i] = '\0';
                //fprintf(stderr, "gpsLogger recvd: %s\n", buf);
                if (!strncmp(buf, cmd, len-2))
                {
                    fprintf(stderr, "gpsLogger: GPS device successfully configured for PPS operation.\n");
                    break;   
                }
            }
            if (strncmp(buf, cmd, len-2))
                fprintf(stderr, "gpsLogger: GPS device PPS configuration failed.\n");
        }
    }
    
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGALRM, SignalHandler); 
    
    memset(&p, 0, sizeof(GPSPosition));
    p.stale = true;
    GPSPublishUpdate(gps_handle, &p);
    // Flush input to make sure we're getting a fresh sentence
    tcflush(serial_fd, TCIFLUSH);
    running = true;
    enum LineStatus {LOW, HI};
    while (running)
    {
        LineStatus dcdCurrent;
        struct timeval pulseTime;
        struct timezone tz;
        if (use_pps)
        {
            tcflush(serial_fd, TCIFLUSH);
            struct itimerval timer;
            timer.it_interval.tv_sec = 10;
            timer.it_interval.tv_usec = 0;
            timer.it_value.tv_sec = 10;
            timer.it_value.tv_usec = 0;
            if (setitimer(ITIMER_REAL, &timer, 0))
            {
                perror("gpsLogger: setitimer() error");      
            }
            // Wait for pulse (change low->hi in DCD)
            if (ioctl(serial_fd, TIOCMIWAIT, TIOCM_CD) < 0)
            {
                perror("gpsLogger: ioctl(TIOCMIWAIT) error");
                continue; 
            }
            gettimeofday(&pulseTime, &tz);
            // Cancel itimer
            timer.it_interval.tv_sec = 0;
            timer.it_interval.tv_usec = 0;
            timer.it_value.tv_sec = 0;
            timer.it_value.tv_usec = 0;
            if (setitimer(ITIMER_REAL, &timer, 0))
            {
                perror("gpsLogger: setitimer() error");      
            }
            // Make sure we caught a low->hi transition
            int status;
            if (ioctl(serial_fd, TIOCMGET, &status) < 0)
            {
                perror("gpsLogger: ioctl(TIOCMGET) error");
                continue;   
            }
            dcdCurrent = (status & TIOCM_CD) ? HI : LOW;
            if (LOW == dcdCurrent) continue;
            // Flush input to make sure we're getting a 
            // fresh sentence after the pulse
            tcflush(serial_fd, TCIFLUSH);
            if (debug) fprintf(stderr, "gpsLogger: caught PPS\n");
        }
        else
        {
            dcdCurrent = HI;
        }

        // OK, DCD just went high, try to read a sentence while
        // before it goes low and high again.
        bool dcdGood = true;
        LineStatus dcdPrevious = dcdCurrent;
        // Do only one settimeofday() or adjtime() per pulse
        bool setTimePending = setTime;
        // Variables used for NMEA sentence seeking/reading
        enum NmeaState 
        {
            SEEKING_SENTENCE, 
            READING_SENTENCE,
            READING_CHECKSUM,
            SENTENCE_COMPLETE
        };
        const unsigned int MAX_SENTENCE_LENGTH = 80;    
        char sentenceBuffer[256];
        unsigned int sentenceLength = 0;
        char checksumBuffer[8];
        unsigned int checksumLength = 0;
        struct timeval sentenceStartTime;
        NmeaState state = SEEKING_SENTENCE;
        
        unsigned int sentenceCount = 0;
        
        while (dcdGood)
        {
            char character;
            int result = read(serial_fd, &character, 1);
            struct timeval currentTime;
            gettimeofday(&currentTime, &tz);
            switch (result)
            {
                case -1:  // error
                    if (EINTR != errno)
                    {
                        perror("gpsLogger: Serial port read error");
                        Cleanup();
                        return false;   
                    }
                    dcdGood = false;  // reset seek for PPS
                    largeTimeChangeFlag = false;  // reset large time change criteria
                    continue;
                    
                case 0:   // eof
                {
                    struct timeval currentTime;
                    struct timezone tz;
                    gettimeofday(&currentTime, &tz);
                    struct tm* theTime = gmtime((time_t*)&currentTime.tv_sec);
                    fprintf(stderr, "gpsLogger: Serial port read timed out! (time>%02d:%02d:%02d.%06lu)\n",
			                                     theTime->tm_hour, 
			                                     theTime->tm_min,
			                                     theTime->tm_sec,
			                                     currentTime.tv_usec);
                    SetStale();
                    dcdGood = false;  // reset seek for PPS
                    largeTimeChangeFlag = false;  // reset large time change criteria
                }
                break;
                    
                default:
                    break;
            }
            
            if (use_pps)
            {
                // Check DCD to make sure we haven't missed a transition
                // (we're polling DCD after each character read)
                int status;
                if (ioctl(serial_fd, TIOCMGET, &status) < 0)
                {
                    perror("gpsLogger: ioctl(TIOCMGET) error");
                    dcdGood = false;  // reset seek for PPS
                    largeTimeChangeFlag = false;  // reset large time change criteria

                }
                else
                {
                    dcdCurrent = (status & TIOCM_CD) ? HI : LOW;
                    if ((LOW == dcdPrevious) && (HI == dcdCurrent))
                    {
                        if (setTimePending)
                            fprintf(stderr, "gpsLogger: Didn't read sentence in time!\n");
                        dcdGood = false;  // reset seek for PPS
                        largeTimeChangeFlag = false;  // reset large time change criteria
                        
                    }
                    else
                    {
                        dcdPrevious = dcdCurrent;
                    }
                }
            }  // end if (use_pps)
            
            // Check published position for "freshness"
            if (!p.stale)
            {
                int deltaTime = currentTime.tv_sec - p.sys_time.tv_sec;
                if (deltaTime > 30) 
                {
                    p.stale = true; 
                    GPSPublishUpdate(gps_handle, &p); 
                } 
            }

            if (dcdGood)
            {
                if (nmeaParse)
                {   
                    // '$' always triggers NMEA sentence start
                    // regardless of current state
                    if ('$' == character)
                    {   
                        if (SEEKING_SENTENCE != state)
                        {
                            fprintf(stderr, "gpsLogger: Warning! prematurely detected new sentence\n");
                            dcdGood = false;  // reset seek for PPS
                            largeTimeChangeFlag = false;  // reset large time change criteria
                        }
                        sentenceStartTime = currentTime;            
                        state = READING_SENTENCE;
                        sentenceLength = 0; 
                    }
                    else if (READING_SENTENCE == state)
                    {
                        if (('*' == character))
                        {
                            state = READING_CHECKSUM;
                            checksumLength = 0;
                        }
                        else if (('\n' == character) ||
                                 ('\r' == character))
                        {
                            if (requireChecksum)
                            {
                                // No checksum when it was required. Complain and throw sentence out.
                                if (debug)
                                {
                                    sentenceBuffer[sentenceLength] = '\0';
                                    fprintf(stderr, "%s\n", sentenceBuffer);
                                }
                                fprintf(stderr, "gpsLogger: Warning! missing expected NMEA checksum.\n");
                                dcdGood = false;  // reset seek for PPS
                                largeTimeChangeFlag = false;  // reset large time change criteria
                                state = SEEKING_SENTENCE;
                            }
                            else
                            {
                                // Checksum may be optional?
                                state = SENTENCE_COMPLETE;   
                            }
                        }
                        else
                        {
                            sentenceBuffer[sentenceLength++] = character;
                            if (sentenceLength > MAX_SENTENCE_LENGTH)
                            {
                                if (debug)
                                {
                                    sentenceBuffer[MAX_SENTENCE_LENGTH] = '\0';
                                    fprintf(stderr, "%s\n", sentenceBuffer);
                                }
                                fprintf(stderr, "gpsLogger: Maximum NMEA sentence length exceeded?\n");
                                dcdGood = false;  // reset seek for PPS
                                largeTimeChangeFlag = false;  // reset large time change criteria
                                state = SEEKING_SENTENCE;   
                            }
                        }                
                    }
                    else if (READING_CHECKSUM == state)
                    {
                        checksumBuffer[checksumLength++] = character;
                        if ((checksumLength > 2) ||
                            ('\n' == character) ||
                            ('\r' == character))
                        {
                            // Try to read checksum and check sentence
                            checksumBuffer[checksumLength] = '\0';
                            int inputChecksum;
                            if (1 == sscanf(checksumBuffer, "%x", &inputChecksum))
                            {
                                unsigned char calculatedChecksum = 0;
                                for (unsigned int i = 0; i < sentenceLength; i++)
                                    calculatedChecksum ^= (unsigned char)sentenceBuffer[i];
                                if (inputChecksum != calculatedChecksum)
                                {
                                    if (debug)
                                    {
                                        sentenceBuffer[sentenceLength] = '\0';
                                        fprintf(stderr, "%s*%s\n", sentenceBuffer, checksumBuffer);
                                    }
                                    fprintf(stderr, "gpsLogger: Bad NMEA checksum!\n");
                                    dcdGood = false;  // reset seek for PPS
                                    largeTimeChangeFlag = false;  // reset large time change criteria
                                    state = SEEKING_SENTENCE;
                                }
                                else
                                {
                                    state = SENTENCE_COMPLETE;
                                }
                            }
                            else
                            {
                                if (debug)
                                {
                                    sentenceBuffer[sentenceLength] = '\0';
                                    fprintf(stderr, "%s*%s\n", sentenceBuffer, checksumBuffer);
                                }
                                fprintf(stderr, "gpsLogger: Bad checksum field!\n");
                                dcdGood = false;  // reset seek for PPS
                                largeTimeChangeFlag = false;  // reset large time change criteria
                                state = SEEKING_SENTENCE;   
                            }
                        }
                    }  // end if/else/else ('$'/READING_SENTENCE/READING_CHECKSUM)

                    // Parse completed NMEA sentence
                    if (SENTENCE_COMPLETE == state)
                    {
                        sentenceCount++;
                        sentenceBuffer[sentenceLength] = '\0';
                        if (debug) fprintf(stderr, "%s\n", sentenceBuffer);
                        
                        // (TBD) We need to age altitude separately
                        // (For now, we save last valid altitude, if applicable
                        bool oldAltitudeIsValid = p.zvalid;
                        double oldAltitude = (oldAltitudeIsValid) ? p.z : 0.0;
                        
                        if (NMEAParser::GetTimeAndPosition(sentenceBuffer, &p))
                        {
                            gettimeofday(&currentTime, &tz);
                            // OK, Got an ACTIVE GPRMC or GPGGA sentence
                            // now set time, log position, etc
                            if (setTimePending && p.tvalid)
                            {
                                setTimePending = false;  // ensures one time adjustment per pulse
                                                         // even with multiple sentences per pulse
                                // Compute current time of day adjustment based on
                                // previously received GPS time (at system time "refTime")
                                // (accounts for serial I/O sentence transmission delay, etc)
                                struct timeval* refTime;
                                if (use_pps)
                                    refTime = &pulseTime;
                                else
                                    refTime = &sentenceStartTime;
                                
                                // Calculate deltaTime using gpsTime and refTime
                                // (note that this trashes the refTime
                                struct timeval deltaTime;
                                // Perform the carry for the later subtraction by updating y
                                if (p.gps_time.tv_usec < refTime->tv_usec) 
                                {
                                    int nsec = (refTime->tv_usec - p.gps_time.tv_usec) / 1000000 + 1;
                                    refTime->tv_usec -= 1000000 * nsec;
                                    refTime->tv_sec += nsec;
                                }
                                if (p.gps_time.tv_usec - refTime->tv_usec > 1000000) 
                                {
                                    int nsec = (refTime->tv_usec - p.gps_time.tv_usec) / 1000000;
                                    refTime->tv_usec += 1000000 * nsec;
                                    refTime->tv_sec -= nsec;
                                }
                                deltaTime.tv_sec = p.gps_time.tv_sec - refTime->tv_sec;
                                deltaTime.tv_usec = p.gps_time.tv_usec - refTime->tv_usec;
                                
                                if (debug) 
                                {
                                    fprintf(stderr, "gpsLogger: currentTime>%lu.%06lu deltaTime>%ld.%06lu\n",
                                                    currentTime.tv_sec, currentTime.tv_usec,
                                                    (long)deltaTime.tv_sec < 0 ? ((long)deltaTime.tv_sec) + 1 :
                                                     deltaTime.tv_sec,
                                                    ((long)deltaTime.tv_sec < 0 ? (1000000-deltaTime.tv_usec) :
                                                    deltaTime.tv_usec ));
                                }
                                
                                bool smallDeltaTime = (0 == deltaTime.tv_sec) || 
                                                      ((long)0xffffffff == deltaTime.tv_sec);
                                
                                if (smallDeltaTime && !forceClock)
                                {
                                    // deltaTime small (i.e. labs(deltaTime) < 1 sec), so use adjtime()  
                                    if (-1 == adjtime(&deltaTime, NULL)) 
                                            perror("gpsLogger: adjtime() error"); 
                                    largeTimeChangeFlag = false;
                                    if (!use_pps) setTime = false;  // only set once if not using PPS
                                }
                                else
                                {
                                    // deltaTime large (i.e. labs(deltaTime) >= 1 second) or (forceClock == true)
                                    forceClock = false;  // we only _force_ settimeofday() use once
                                            
                                    bool changeTime;
                                    
                                    if (smallDeltaTime)
                                    {
                                        largeTimeChangeFlag = 0;
                                        changeTime = true;
                                    }
                                    else
                                    {
                                        // We only call settimeodday() if we get 2 consecutive readings
                                        // from the GPS device with a similar large delta between the
                                        // GPS time and the system time
                                        // The "largeTimeChangeFlag" marks the first large delta reading
                                        if (largeTimeChangeFlag)
                                        {
                                            if (10 > labs(largeTimeChangeDelta - deltaTime.tv_sec))
                                            {
                                                // Consistent large delta from GPS
                                                changeTime = true;
                                                fprintf(stderr, "gpsLogger: Warning: attempting time change of 1 second or more ...\n");

                                            }
                                            else
                                            {
                                                // Inconsistent delta, possibly corrupt GPS data
                                                changeTime = false;
                                            }
                                            largeTimeChangeFlag = false;
                                        }
                                        else
                                        {
                                            fprintf(stderr, "gpsLogger: Warning: delaying time change of 1 second or more ...\n");
                                            largeTimeChangeFlag = true; 
                                            largeTimeChangeDelta = deltaTime.tv_sec;
                                            changeTime = false;
                                        }
                                    }
                                    
                                    if (changeTime)
                                    {
                                        if (!use_pps) setTime = false;  // only set once if not using PPS
                                        long offsetSec = currentTime.tv_sec - refTime->tv_sec;
                                        long offsetUsec = currentTime.tv_usec - refTime->tv_usec;
                                        if (offsetUsec < 0)
                                        {
                                            offsetSec--;
                                            offsetUsec += 1000000;
                                        }
                                        currentTime.tv_sec = p.gps_time.tv_sec + offsetSec;
                                        currentTime.tv_usec = p.gps_time.tv_usec + offsetUsec;
                                        if (currentTime.tv_usec > 999999)
                                        {                           
                                            currentTime.tv_sec++;   
                                            currentTime.tv_usec -= 1000000;
                                        }
                                        if (-1 == settimeofday(&currentTime, &tz)) 
                                            perror("gpsLogger: settimeofday() error");
                                    }  // end if (changeTime)
                                }  // end if/else (smallDeltaTime)
                            }  // end if (setTime && p.tvalid)
                            
                            if (!p.zvalid && oldAltitudeIsValid) 
                                p.z = oldAltitude;
                            
                            if (logging)
                            {
                                struct tm* theTime = gmtime((time_t*)&currentTime.tv_sec);
                                fprintf(log_ptr, "time>%02d:%02d:%02d.%06lu position>%f,%f,%f\n",
			                                     theTime->tm_hour, 
			                                     theTime->tm_min,
			                                     theTime->tm_sec,
			                                     currentTime.tv_usec,
                                                 p.y, p.x, p.z);
                            }
                            p.sys_time = currentTime;
                            p.stale = false;
                            GPSPublishUpdate(gps_handle, &p);
                        }
                        else
                        {
                            // Non-useful sentence for whatever reason
                            // (e.g. non-GPRMC or non-GPGGA sentence, VOID sentence, etc)
                        }  // end if/else NMEAParser::GetTimeAndPosition()
                        if(sentenceCount > 3) dcdGood = false;
                        state = SEEKING_SENTENCE;
                    }  // end if (SENTENCE_COMPLETE)
                }  
                else
                {
                    // (TBD) "binary" parsing goes here   
                }  // end if/else (nmeaParse)
            }  // end if (dcdGood)
        }  // end while(dcdGood)
    }  // end while(running)
    Cleanup();
    return true;
}  // end GPSLogger::Main()

void GPSLogger::SignalHandler(int sigNum)
{
    switch(sigNum)
    {
        case SIGTERM:
        case SIGINT:
            theApp.Stop();
            break;
            
        case SIGALRM:
        {
            theApp.SetStale();
            struct timeval currentTime;
            struct timezone tz;
            gettimeofday(&currentTime, &tz);
            struct tm* theTime = gmtime((time_t*)&currentTime.tv_sec);
            fprintf(stderr, "gpsLogger: Serial port PPS timed out! (time>%02d:%02d:%02d.%06lu)\n",
			                             theTime->tm_hour, 
			                             theTime->tm_min,
			                             theTime->tm_sec,
			                             currentTime.tv_usec);
        }
        break;
            
        default:
            fprintf(stderr, "gpsLogger: Unexpected signal: %d\n", sigNum);
            break; 
    }  
}  // end GPSLogger::SignalHandler()

void GPSLogger::SetStale()
{
    p.stale = true;
    GPSPublishUpdate(gps_handle, &p);
}  // end GPSLogger::SetStale()

void GPSLogger::Stop()
{
    running = false;
    Cleanup();
    fprintf(stderr, "gpsLogger: Done.\n");
    exit(0);
}  // end GPSLogger::Stop()

void GPSLogger::Cleanup()
{
    if (serial_fd >= 0)
    {
        close(serial_fd);
        serial_fd = -1;
    }
    if (log_ptr)
    {
        fclose(log_ptr);
        log_ptr = NULL;
    }
    if (gps_handle)
    {
         GPSPublishShutdown(gps_handle, NULL);
         gps_handle = NULL;   
    }
}  // end GPSLogger::Cleanup()

void GPSLogger::Usage()
{
    fprintf(stderr, "gpsLogger Version %s\n", VERSION);
    fprintf(stderr, "Usage: gpsLogger [setTime][pps][noLog][log <logFile>]\n"
                    "                 [device <serialDevice>][speed <baud>][gps35]"
                    "                 [pubFile <pubFile>]\n");
}
