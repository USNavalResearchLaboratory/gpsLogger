                        GPSLOGGER      

gpsLogger does the following:

1) Monitors a serial port for NMEA sentences with GPS time
   and position information.

2) Optionally sets the computer time-of-day according to
   valid incoming GPS timekeeping.  If PPS is available,
   gpsLogger can use it to have the system time track the
   GPS device time on a continual basis.  It isn't yet as
   sophisticated as "ntpd" in this regards, but can provide
   a very good time of day reference anyway.

3) Outputs a log of GPS position with respect to system time 
   to <stdout> or a log file.  (Logging can be disabled)

4) Publishes the _current_ GPS position (and time of the
   fix) to shared memory using the routines defined in
   "gpsPub.h".
   
FILES:

README.txt      - this file.

gpsLogger.html  - gpsLogger User Guide web page
                  (more detailed info than here)

gpsLogger.cpp   - gpsLogger program source code (C++)

nmeaParse.h     - Routines for parsing NMEA sentences
nmeaParse.cpp

gpsPub.h        - Routines for GPS position publish/subscribe
gpsPub.cpp        (using shared memory)

gpsClient.cpp   - Example source code for using GPSSubscribe()

gpsFaker.cpp    - Program to publish "fake" GPS position to
                  shared memory.

gpsReport.pl    - Perl script to analyze gpsLogger logs and (optionally)
                  a file that is a capture of stdin/stdout via
                  "gpsLogger [options] > <errorlogfile> 2>&1.
                  It collects statistics on how many of each of the
                  different errors occured, whether time went backwards,
                  if it went forward by more than 2 seconds, etc.
                  Type "gpsReport.pl --help" for usage/syntax.  

TO BUILD:                   
       g++ -o gpsLogger gpsLogger.cpp gpsPub.cpp nmeaParse.cpp             
 
 
USAGE:

gpsLogger [set][pps][force][check][gps35][noLog][log <logFile>]
          [debug][device <serialDevice>][speed <baud>]
          [pubFile <pubFile>]
          
set      - cause "gpsLogger" to set system time upon
          reciept of first valid NMEA sentence with
          _active_ GPS position fix or if the "pps"
          option is used, continually adjust time
          to track GPS time.
          
force    - cause "gpsLogger" to perform a hard "settimeofday()"
           to _force_ the system clock into sync with GPS
           immediately instead of gradually converging towards
           GPS time using the system "adjtime()" function.
           (Note that if the delta(systemTime, gpsTime) is
           greater than or equal to one second, "gpsLogger"
           will force the time to hard adjust)
          
pps      - cause "gpsLogger" to wait for pulse-per-second
          (PPS) signal on the serial port DCD (or optionally
          the CTS) pin.

cts      - cause "gpsLogger" to use clear-to-send (CTS) signal
           for PPS signal instead of default DCD pin.
           
invert   - By default, "gpsLogger" waits for a low-to-hi transition
           of the PPS signal for pulse detection.  The "invert"
           option here inverts this logic to look for a
           hi-to-low transition instead.

gps35    - cause "gpsLogger" to send configuration sentence
          for Garmin GPS-35 units to turn on PPS operation.
          (This _may_ work with other Garmin units, too)

check    - require that received NMEA sentences have a checksum
           or else ignore the sentence

noLog    - disables logging of position.

log <logFile>         - log to file instead of <stdout>

debug    - cause "gpsLogger" to output additional debugging
           information to stderr

device <serialDevice> - Monitor <serialDevice>. 
                        "/dev/ttyS0" is the default.

speed <baud>          - Set serial port baud rate to 4800 or
                        9600.  4800 is the default.

pubFile <pubFile>     - Name of file with GPSPub shared
                        memory identifier. Default is
                        "/tmp/gpskey"
                        

KNOWN ISSUES:

1) Add a "bool GPSSubscriptionIsValid(GPSHandle gpsHandle)"
   function to GPSPub routines so client can check to make
   sure the shared memory segment is still being actively
   updated ... i.e. check, using "shmctl()" to see if the
   segment is marked for destruction.  This way the clients
   can detect if "gpsLogger" has been killed.

