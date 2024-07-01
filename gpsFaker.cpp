#include "gpsFaker.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <csignal>
#include <cstring>
#include <cerrno>

#include <unistd.h>

using namespace std;

/* Bug in old versions of GCC?
using std::atof;
using std::atol;
using std::errno;
using std::strcmp;
using std::memcpy;
*/

GPSFaker* GPSFaker::faker = 0;

int main (int argc, char* argv[])
{
 GPSFaker::main(argc, argv);
}

void GPSFaker::main (int argc, char* argv[])
{
 std::signal(SIGINT, signalHandler);
 std::signal(SIGTERM, signalHandler);

 FakeDataGenerator* gen;
 
 if (argc < 2)
  exitWithError("Need mode.");
 
 if (strcmp(argv[1], "static") == 0)
 {
  if (argc < 4)
   exitWithError("Must specify lat/long.");
   
  // argv[2] is longitude
  errno = 0;
  double longi = atof(argv[2]);
  if (errno != 0)
   exitWithError("Bad longitude");
    
  // argv[3] is latitude
  double lat = atof(argv[3]);
  if (errno != 0)
   exitWithError("Bad latitude");
  
  // argv[4] is flaky
  bool flaky;
  if (argc < 5)
   flaky = false;
  else if (strcmp(argv[4], "flaky") == 0)
   flaky = true;
  else
   exitWithError("Invalid argument");
  
  gen = new StaticGenerator(flaky, longi, lat);
 }
 else if (strcmp(argv[1], "line") == 0)
 {
  // argv[2] is start long
  double start_long = atof(argv[2]);
  if (errno != 0)
   exitWithError("Bad start long");
  
  // argv[3] is start lat
  double start_lat = atof(argv[3]);
  if (errno != 0)
   exitWithError("Bad start lat");
  
  // argv[4] is end long
  double end_long = atof(argv[4]);
  if (errno != 0)
   exitWithError("Bad end long");

  // argv[5] is end lat
  double end_lat = atof(argv[5]);
  if (errno != 0)
   exitWithError("Bad end lat");

  // argv[6] is time
  long time = atol(argv[6]);
  if (errno != 0)
   exitWithError("Bad time");
  
  // argv[7] is flaky
  bool flaky;
  if (argc < 8)
   flaky = false;
  else if (strcmp(argv[7], "flaky") == 0)
   flaky = true;
  else
   exitWithError("Invalid argument");
  
  gen = new LineGenerator(flaky, start_long, start_lat, end_long, end_lat,
                          time);
 }
 
 cout << "Creating faker..." << endl;
 
 faker = new GPSFaker(gen);
 if (faker->ready())
  faker->run();
 else
  exitWithError("Cannot setup faker.");
}

void GPSFaker::exitWithoutError ()
{
 if (faker != 0)
  delete faker;
 std::exit(0);
 return;
}

void GPSFaker::exitWithError (char* err)
{
 std::cout << "Error: " << err << std::endl;
 
 if (faker != 0)
  delete faker;
  
 std::exit(1);
 return;
}

void GPSFaker::signalHandler (int signum)
{
 exitWithoutError();
}

GPSFaker::GPSFaker (FakeDataGenerator* gen) 
 : generator(gen)
{
 handle = GPSPublishInit(0);
}

GPSFaker::~GPSFaker ()
{
 GPSPublishShutdown(handle, 0);
}

bool GPSFaker::ready ()
{
 return handle;
}

void GPSFaker::run ()
{
 struct timeval time;
 GPSPosition pos;

 pos.z = 0;
 pos.zvalid = true;
 pos.xyvalid = true;
 pos.tvalid = true;
   
 while (true)
 {
  // Update time
  gettimeofday(&time, 0);
  generator->updateTime(&time);
    
  // Get new position
  pos.x = generator->getLongitude();
  pos.y = generator->getLatitude();
  memcpy(&pos.gps_time, &time, sizeof(struct timeval));
  memcpy(&pos.sys_time, &time, sizeof(struct timeval));
  pos.stale = generator->getValid() ? false : true;
  
  // Store position
  GPSPublishUpdate(handle, &pos);
  
  // Delay
  sleep(1);
 }
 
 return;
}

StaticGenerator::StaticGenerator (bool new_flaky, double longi, double lat)
 : flaky(new_flaky), latitude(lat), longitude(longi)
{}
 
void StaticGenerator::updateTime (const struct timeval* new_time)
{}
 
double StaticGenerator::getLatitude ()
{
 return latitude;
}

double StaticGenerator::getLongitude ()
{
 return longitude;
} 

bool StaticGenerator::getValid ()
{
 if (!flaky)
  return true;
  
 // 75% valid
 return std::rand() < (int) std::floor((double) RAND_MAX * 0.75);
}

LineGenerator::LineGenerator (bool new_flaky, double start_long, double start_lat,
                              double end_long, double end_lat, long total_time)
 : flaky(new_flaky), curLatitude(start_lat), curLongitude(start_long), 
   startLatitude(start_lat), startLongitude(start_long),
   endLatitude(end_lat), endLongitude(end_long), totalTime(total_time)
{
 gettimeofday(&startTime, 0);
}

void LineGenerator::updateTime (const struct timeval* new_time)
{
 long elapsed = new_time->tv_sec - startTime.tv_sec;
 double frac = (double) elapsed / (double) totalTime;
 
 curLatitude += frac * (endLatitude - startLatitude);
 curLongitude += frac * (endLongitude - startLongitude);
 return;
}

double LineGenerator::getLatitude ()
{
 return curLatitude;
}

double LineGenerator::getLongitude ()
{
 return curLongitude;
}

bool LineGenerator::getValid ()
{
 if (!flaky)
  return true;
  
 // 75% valid
 return std::rand() < (int) std::floor((double) RAND_MAX * 0.75);
}
