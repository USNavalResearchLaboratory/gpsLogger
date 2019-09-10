/** Kenneth W. Flynn
 *  GPSFaker.h
 */
 
#ifndef __GPSFAKER_H
#define __GPSFAKER_H

#include "gpsPub.h"
 
#include <sys/time.h>
 
class FakeDataGenerator;

class GPSFaker
{
 public:
  static void main (int arg, char* argv[]);
  static void exitWithoutError ();
  static void exitWithError (char* err);
  static void signalHandler (int signum);
  
  static GPSFaker* faker;
  
 public:
  GPSFaker(FakeDataGenerator* gen);
  ~GPSFaker();
  
  bool ready ();
  void run ();
  
 private:
  FakeDataGenerator* generator;
  GPSHandle handle;
};

// Interface for fake data generators
class FakeDataGenerator
{
 public:
  virtual void updateTime (const struct timeval* new_time) = 0;
  virtual double getLatitude () = 0;
  virtual double getLongitude () = 0; 
  virtual bool getValid () = 0;
};

class StaticGenerator : public FakeDataGenerator
{
 public:
  StaticGenerator (bool flaky, double longi, double lat);
  virtual ~StaticGenerator () {}
  
  virtual void updateTime (const struct timeval* new_time);
  virtual double getLatitude ();
  virtual double getLongitude ();
  virtual bool getValid ();
  
 private:
  bool flaky;
  double latitude;
  double longitude;
};

class LineGenerator : public FakeDataGenerator
{
 public:
  LineGenerator(bool flaky, double start_long, double start_lat, 
                double end_long, double end_lat, long total_time);
  virtual ~LineGenerator () {}

  virtual void updateTime (const struct timeval* new_time);
  virtual double getLatitude ();
  virtual double getLongitude ();
  virtual bool getValid ();
                
 private:
  bool flaky;
  double curLatitude;
  double curLongitude;
  double startLatitude;
  double startLongitude;
  double endLatitude;
  double endLongitude;
  long totalTime;
  struct timeval startTime;
};

#endif
