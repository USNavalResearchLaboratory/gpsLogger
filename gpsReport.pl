#!/usr/bin/perl -w

# gpsReport.pl Perl Script
# created by Jeff Weston (weston@itd.nrl.navy.mil)

$version = "1.03";

# Version Hisory
# 1.00:  09/09/02 -- initial release.
# 1.01:  09/10/02 -- fixed a couple stupid syntax errors and very minor bugs, got rid of warnings.
# 1.02:  09/11/02 -- Added some more usage comments and changed USAGE error messages.
# 1.03:  09/12/02 -- Fixed some minor bugs, fixed help message formatting.

# Check commandline arguments 
# gpsReport.pl version or gpsReport.pl -version or gpsReport.pl --version => print version number, exit.
# gpsReport.pl -help or gpsReport.pl --help => print Usage instruction, exit.
# gpsReport.pl => both gps file and error file are in current directory, named gps.log and gpsLogger.output.
# gpsReport.pl <gpslog> <errorlog> => new locations (absolute or relative),  complete with names.
# gpsReport.pl <gpslog> => gps log and error log are the same file, or there is no error file (in which case #errors will be incorrect, and 0).
# gpsReport.pl [options] -derivative or --derivative option => plot derivative of GPS Time Ramp function, along with everything else.

$mergedlogs = 0;	# change to 1 if gps log and error log are same file.
$plotderivative = 0;	# change to 1 to plot the derivatives along with everything else.
$argnumber = 0;		# counter for argument number
$locationcounter = 1;	# 1 = first input log file, 2 = second input log file
$gpsfile = "gps.log";
$errorfile = "gpsLogger.output";

if ( @ARGV )
{
	while ( $ARGV[$argnumber] )
	{
		if( $ARGV[$argnumber] =~ /version/ )
		{
			die "gpsReport version: $version\n";
		}
		elsif ( $ARGV[$argnumber] =~ /-help/ )
		{
			print "\nUsage: gpsReport.pl [--version] [--help] [<gps_log_file>] [<error_log_file>]\n";
			print " [--derivative]\n\n";
			print "   <gps_log_file> and <error_log_file> are the absolute and/or relative\n";
			print "   locations for those files, along with the names.  If no locations are given,\n";
			print "   the program assumes they are named 'gps.log' and 'gpsLogger.output', and that\n";
			print "   they are in the current working directory.  If only one file name/location is\n";
			print "   given, then it will run assuming that both regular gpsLogger output and the\n";
			print "   error messages are in that file, and if this is not the case, the results may\n";
			print "   be invalid.\n\n";
			print "gpsReport Options:\n";
			print "   --version       print version number, exit.\n";
			print "   --help          print this message.\n";
			print "   --derivative    also plots the derivative of GPS Time Ramp function.\n\n";
			print "Output Files:\n";
			print "   gpsReport.gnuplot           Time ramp plot (and optional derivative).\n";
			print "                               Use 'gnuplot -persist gpsReport.gnuplot' to view\n";
			print "                               this plot.\n";
			print "   gpsReport.largedelta        Log file with all time jumps > 2 seconds.\n";
			print "   gpsReport.results           Overall results (printed automatically after\n";
			print "                               running gpsReport.\n";
			print "   gpsReport.reversetime       Log file with several sentences around any time\n";
			print "                               period during which time went backwards.\n";
			print "   gpsReport.unexpectedoutput  Log file with any strange messages that are not\n";
			print "                               normal errors or output.\n\n";
			die "Use Shift-Page Up to see the beginning of this help file.\n";
		}
                elsif ( $ARGV[$argnumber] =~ /-derivative/ )
                {
                        $plotderivative = 1;
                }
		else
		{
			if ( $locationcounter == 1 )
			{
				$gpsfile = $ARGV[$argnumber];
				$locationcounter += 1;
				$mergedlogs = 1;
			}
			elsif ( $locationcounter == 2 )
			{
				$errorfile = $ARGV[$argnumber];
				$locationcounter += 1;
				$mergedlogs = 0;
			}
			else
			{
				die "Incorrect syntax.\nUsage:  gpsReport.pl [--version] [--help] [<gps_log_file>] [<error_log_file>] [-derivative]\n";
			}
		}
		$argnumber += 1;
	}
}


# Open files

-e $gpsfile or die "File $gpsfile does not exist!\n";
-r $gpsfile or die "File $gpsfile cannot be read!\n";
-T $gpsfile or die "File $gpsfile is not a text file!\n";
open(GPSFILE, $gpsfile) or die "Can't open file $gpsfile!\n";
if ( $mergedlogs == 0 )		# if logs are NOT the same file
{
	-e $errorfile or die "File $errorfile does not exist!\n";
	-r $errorfile or die "File $errorfile cannot be read!\n";
	-T $errorfile or die "File $errorfile is not a text file!\n";
	open(ERRORFILE, $errorfile) or die "Can't open file $errorfile!\n";
}
open(REPORTFILE, ">gpsReport.results") or die "Can't create file gpsReport.results!\n";
open(LARGETIMECHANGEREPORTFILE, ">gpsReport.largedelta") or die "Can't create file gpsReport.largedelta!\n";
open(REVERSETIMEREPORTFILE, ">gpsReport.reversetime") or die "Can't create file gpsReport.reversetime!\n";
open(UNEXPECTEDOUTPUTREPORTFILE, ">gpsReport.unexpectedoutput") or die "Can't create file gpsReport.unexpectedoutput!\n";
open(PLOTFILE, ">gpsReport.gnuplot") or die "Can't create file gpsReport.gnuplot!\n";
open(DERIVATIVEPLOTFILE, ">gpsReport.dplot") or die "Can't create file gpsReport.dplot!\n";

# COUNTER INITIALIZATION

# general counters
$sentencecounter = 0;		# number of valid sentences in file
$errorcounter = 0;		# number of errors in file
$invalidcounter = 0;		# number of invalid things in file (not time/error)

# time counters
$reversetimecounter = 0;	# number of times time goes backwards
$largetimechangecounter = 0;	# number of times time changes by more than 1 second
$dayrollover = 0;		# number of date rollovers
$newdaycounter = 0;		# number of sentences supposedly from a new day
$endofdaycounter = 0;		# number of sentences supposedly from near the end of a day
$twolinecounter = 0;		# counter for printing next two lines after reverse time has been sensed

# error counters
$nmeaparsererror = 0;		# number of NMEA Parser errors
$serialporttimeout = 0;		# number of serial port read timeouts
$serialportrwerror = 0;		# number of serial port read/write errors
$prematuresentence = 0;		# number of prematurely detected new sentence errors
$nosentenceintime = 0;		# number of didn't read sentence in time errors
$missingcheck = 0;		# number of missing checksum errors
$longsentence = 0;		# number of maximum sentence length exceeded errors
$badnmeacheck = 0;		# number of bad NMEA checksum errors
$badcheckfield = 0;		# number of back checksum field errors
$timechangeattempt = 0;		# number of time change attempts
$timechangedelay = 0;		# number of time change delays
$settimeofdayerror = 0;		# number of set time of day errors
$strangeerror = 0;		# number of unknown/unexpected errors

# OTHER VARIABLES

$currenthour = 0;               # current hour value
$currentmin = 0;                # current minute value
$currentsec = 0;                # current second value
$lasthour = 0;                  # last hour value
$lastmin = 0;                   # last minute value
$lastsec = 0;                   # last second value
$deltatime = 0;			# difference between current time and previous time (seconds)
$minssincestart = 0;		# difference between current time and first time value in logfile (minutes)
$lastminssincestart = 0;	# $minssincestart of last valid time sentence
$slope = 0;			# slope for derivative plot
$dayrolloverimminent = 0;	# flag saying that a day rollover can occur soon
$printnexttwolines = 0;		# flag (when = 1) telling program to print next two valid time sentences after reverse time sensed
$firsttimesentenceflag = 1;     # 1 = have not yet gotten time sentence, 0 = has gotten time sentenc
$firsttime = 0;			# holds the time (in seconds since midnight) of the first valid time in the log file
#@backwardstimearray;		# holds the times and corresponding $sentencecounter value for each time when time went backwards.
$backwardstimeindex = 0;	# counter for current index of @backwardstimearray

$currentsentence = 0;		# stores current valid gps time sentence
$lastsentence = 0;		# stores last valid gps time sentence
$beforelastsentence = 0;	# stores the last valid gps time sentence before $lastsentence

$ppssuccess = -1;		# 1=PPS success, 0=PPS failure, -1=neither message appeared
$ppserror = 0;			# 1=more than one PPS success/failure message

# Set up PLOTFILE initialization items.
print PLOTFILE "set title 'gpsReport Time Ramp Plot'\n";
print PLOTFILE "set xlabel 'Sentence Index'\n";
print PLOTFILE "set ylabel 'Time (min)'\n";
print PLOTFILE "set key top left\n";
print PLOTFILE "plot 'gpsReport.gnuplot' index 1 using 1:2 t 'Time Ramp' with lines\n";
if ( $plotderivative == 1 )
{
	print PLOTFILE "replot 'gpsReport.gnuplot' index 2 using 1:2 t 'd/dt(Time Ramp)' with steps\n";
}
else
{
	print PLOTFILE "replot 'gpsReport.gnuplot' index 3 using 1:2 t 'N/A' with points pt 4 ps 5\n";
}
print PLOTFILE "replot 'gpsReport.gnuplot' index 3 using 1:2 t 'Backwards Times' with points pt 4 ps 5\n";
print PLOTFILE "exit\n\n\n";

# Go through GPSFILE

while (<GPSFILE>)
{
	if ( $_ =~ /NMEAParser:/ )
	{
		$nmeaparsererror += 1;
		$errorcounter += 1;
	}
	elsif ( ( $_ =~ /gpsLogger: configuring GPS device for PPS operation/ ) || ( $_ =~ /Done./ ) || ( $_ =~ /deltaTime/ ) || ( $_ =~ /caught PPS/ ))
	{
		# ignore, these are normal and should appear in most files
	}
	elsif ( $_ =~ /gpsLogger:/ )	# all other errors
	{
		$errorcounter += 1;
		if ( ( ( $_ =~ /Serial port/ ) || ( $_ =~ /serial port/ ) ) && ( $_=~ /timed out/ ) )
		{
			$serialporttimeout += 1;
		}
		elsif ( ( $_ =~ /serial port write/ ) || ( $_ =~ /Serial port read/ ) || ( $_ =~ /Serial port write/ ) || ( $_ =~ /serial port read/ ) )
		{
			$serialportrwerror += 1;
		}
		elsif ( $_ =~ /GPS device successfully configured for PPS operation/ )
		{
			$errorcounter -= 1;		# not really an error, subtract added error out
			if ( $ppssuccess != -1 )
			{
				$ppserror = 1;
			}
			$ppssuccess = 1;
		}
		elsif ( $_ =~ /GPS device PPS configuration failed/ )
		{
			$errorcounter -= 1;             # not really an error, subtract added error out.
			if ( $ppssuccess != -1 )
			{
				$ppserror = 1;
			}
			$ppssuccess = 0;
		}
		elsif ( $_ =~ /Didn't read sentence in time!/ )
		{
			$nosentenceintime += 1;
		}
		elsif ( $_ =~ /missing expected NMEA checksum/ )
		{
			$missingcheck += 1;
		}
		elsif ( $_ =~ /prematurely detected new sentence/ )
		{
			$prematuresentence += 1;
		}
		elsif ( $_ =~ /Maximum NMEA sentence length exceeded/ )
		{
			$longsentence += 1;
		}
		elsif ( $_ =~ /Bad NMEA checksum!/ )
		{
			$badnmeacheck += 1;
		}
		elsif ( $_ =~ /Bad checksum field!/ )
		{
			$badcheckfield += 1;
		}
		elsif ( $_ =~ /attempting time change/ )
		{
			$timechangeattempt += 1;
			$errorcounter -= 1;		# not necessarily an error, subtract back out
		}
		elsif ( $_ =~ /delaying time change/ )
                {
                        $timechangedelay += 1;
			$errorcounter -= 1;             # not necessarily an error, subtract back out
                }
                elsif ( $_ =~ /settimeofday() error/ )
                {
                        $settimeofdayerror += 1;
                }
                else
                {
                        $strangeerror += 1;
			print UNEXPECTEDOUTPUTREPORTFILE;
                }
	}
	elsif ( $_ =~ /time>([0-9]+):([0-9]+):([0-9]+\.[0-9]+).*/ )	# valid sentence beginning
	{
		$sentencecounter += 1;
		$currentsentence = $_;
		# check for reverse time print flag
		if ( $printnexttwolines == 1 )
		{
			print REVERSETIMEREPORTFILE;
			$twolinecounter -= 1;
			if ( $twolinecounter == 0 )
			{
				$printnexttwolines = 0;
				print REVERSETIMEREPORTFILE "\n";
			}
		}
		# extract the numbers from the pattern matching above
		$currenthour = $1;
		$currentmin = $2;
		$currentsec = $3;

		# detect date rollover.
		# NOTE:  this is not perfect.  But it really can't be without date info in the gps logs.
		if ( ( $currenthour == 23 ) && ( $currentmin = 59 ) )
		{
			# we are supposedly in the last minute of a given day
			if ( $dayrolloverimminent == 0 )
			{
				$endofdaycounter += 1;
				if ( $endofdaycounter == 10 )
				{
					$dayrolloverimmenent = 1;
					$endofdaycounter = 0;
				}
			}
		}
		else
		{
			$endofdaycounter = 0;
		}

		if ( $dayrolloverimminent == 1 )
		{
			# look for data from new day
			if ( ( $currenthour == 0 ) && ( $currentmin == 0 ) )
			{
				# we are supposedly in the first minute of a new day
				$newdaycounter += 1;
				if ( $newdaycounter == 10 )
				{
					# ten sentences in a row from the first minute of a new day.  Assume date rollover has occured
					$dayrollovercounter += 1;
					$dayrolloverimminent = 0;
					$newdaycounter = 0;
				}
			}
			else
			{
				$newdaycounter = 0;
			}
		}
		# end of date rollover detection

		if ( $firsttimesentenceflag == 1 )	# if first sentence in the log
		{
			$firsttime = ( ( ( ( $currenthour * 60 ) + $currentmin ) * 60 ) + $currentsec );
			$firsttimesentenceflag = 0;
			print PLOTFILE "$sentencecounter, 0\n";
		}
		else	# not first sentence in log
		{
			# compare and put in PLOTFILE
			$minssincestart = ( ( ( ( ( ( ( ( $dayrollover * 24 ) + $currenthour ) * 60 ) + $currentmin ) * 60 ) + $currentsec ) - $firsttime ) / 60 );
			print PLOTFILE "$sentencecounter, $minssincestart\n";
			$slope = $minssincestart - $lastminssincestart;
			print DERIVATIVEPLOTFILE "$sentencecounter, $slope\n";

			# compare against last time
			$deltatime = ( ( ( ( ( ( $dayrollover * 24 ) + $currenthour - $lasthour ) * 60 ) + $currentmin - $lastmin ) * 60 ) + $currentsec - $lastsec );
			if ( $deltatime > 2 )	# "large" time change
			{
				$largetimechangecounter += 1;
				# print out previous and current times
				print LARGETIMECHANGEREPORTFILE $lastsentence;
				print LARGETIMECHANGEREPORTFILE "$currentsentence\n";
			}
			elsif ( $deltatime < 0 )	# time went backwards
			{
				$reversetimecounter += 1;
				# store in array for later usage in plotting
				$backwardstimearray[$backwardstimeindex] = $sentencecounter;
				$backwardstimearray[$backwardstimeindex + 1] = $minssincestart;
				$backwardstimeindex += 2;

				# check for already going negative and printing result
				if ( $printnexttwolines == 1 )
				{
					$twolinecounter = 2;
					# don't print current or last ones, they're already printed
				}
				else
				{
					print REVERSETIMEREPORTFILE $beforelastsentence;
					print REVERSETIMEREPORTFILE $lastsentence;
					print REVERSETIMEREPORTFILE $currentsentence;
					$printnexttwolines = 1;
					$twolinecounter = 2;
				}
			}
		}
		$lasthour = $currenthour;
	        $lastmin = $currentmin;
        	$lastsec = $currentsec;
	        $beforelastsentence = $lastsentence;
        	$lastsentence = $currentsentence;
		$lastminssincestart = $minssincestart;
	}
	else	# invalid sentence
	{
		print UNEXPECTEDOUTPUTREPORTFILE;
		$invalidcounter += 1;
	}
}

# Go through ERRORFILE

if ( $mergedlogs == 0 )		# if logs are not same file
{
while (<ERRORFILE>)
{
	if ( $_ =~ /NMEAParser:/ )
	{
		$nmeaparsererror += 1;
		$errorcounter += 1;
	}
	elsif ( ( $_ =~ /gpsLogger: configuring GPS device for PPS operation/ ) || ( $_ =~ /Done./ ) || ( $_ =~ /deltaTime/ ) || ( $_ =~ /caught PPS/ ))
	{
		# ignore, these are normal and should appear in most files
	}
	elsif ( $_ =~ /gpsLogger:/ )	# all other errors
	{
		$errorcounter += 1;
		if ( ( ( $_ =~ /Serial port/ ) || ( $_ =~ /serial port/ ) ) && ( $_=~ /timed out/ ) )
		{
			$serialporttimeout += 1;
		}
		elsif ( ( $_ =~ /serial port write/ ) || ( $_ =~ /Serial port read/ ) || ( $_ =~ /Serial port write/ ) || ( $_ =~ /serial port read/ ) )
		{
			$serialportrwerror += 1;
		}
		elsif ( $_ =~ /GPS device successfully configured for PPS operation/ )
		{
			$errorcounter -= 1;		# not really an error, subtract added error out
			if ( $ppssuccess != -1 )
			{
				$ppserror = 1;
			}
			$ppssuccess = 1;
		}
		elsif ( $_ =~ /GPS device PPS configuration failed/ )
		{
			$errorcounter -= 1;             # not really an error, subtract added error out.
			if ( $ppssuccess != -1 )
			{
				$ppserror = 1;
			}
			$ppssuccess = 0;
		}
		elsif ( $_ =~ /Didn't read sentence in time!/ )
		{
			$nosentenceintime += 1;
		}
		elsif ( $_ =~ /missing expected NMEA checksum/ )
		{
			$missingcheck += 1;
		}
		elsif ( $_ =~ /prematurely detected new sentence/ )
		{
			$prematuresentence += 1;
		}
		elsif ( $_ =~ /Maximum NMEA sentence length exceeded/ )
		{
			$longsentence += 1;
		}
		elsif ( $_ =~ /Bad NMEA checksum!/ )
		{
			$badnmeacheck += 1;
		}
		elsif ( $_ =~ /Bad checksum field!/ )
		{
			$badcheckfield += 1;
		}
		elsif ( $_ =~ /attempting time change/ )
		{
			$timechangeattempt += 1;
			$errorcounter -= 1;		# not necessarily an error, subtract back out
		}
		elsif ( $_ =~ /delaying time change/ )
                {
                        $timechangedelay += 1;
			$errorcounter -= 1;             # not necessarily an error, subtract back out
                }
                elsif ( $_ =~ /settimeofday() error/ )
                {
                        $settimeofdayerror += 1;
                }
                else
                {
                        $strangeerror += 1;
			print UNEXPECTEDOUTPUTREPORTFILE;
                }
	}
	elsif ( $_ =~ /time>([0-9]+):([0-9]+):([0-9]+\.[0-9]+).*/ )	# valid sentence beginning
	{
		$sentencecounter += 1;
		$currentsentence = $_;
		# check for reverse time print flag
		if ( $printnexttwolines == 1 )
		{
			print REVERSETIMEREPORTFILE;
			$twolinecounter -= 1;
			if ( $twolinecounter == 0 )
			{
				$printnexttwolines = 0;
				print REVERSETIMEREPORTFILE "\n";
			}
		}
		# extract the numbers from the pattern matching above
		$currenthour = $1;
		$currentmin = $2;
		$currentsec = $3;

		# detect date rollover.
		# NOTE:  this is not perfect.  But it really can't be without date info in the gps logs.
		if ( ( $currenthour == 23 ) && ( $currentmin = 59 ) )
		{
			# we are supposedly in the last minute of a given day
			if ( $dayrolloverimminent == 0 )
			{
				$endofdaycounter += 1;
				if ( $endofdaycounter == 10 )
				{
					$dayrolloverimmenent = 1;
					$endofdaycounter = 0;
				}
			}
		}
		else
		{
			$endofdaycounter = 0;
		}

		if ( $dayrolloverimminent == 1 )
		{
			# look for data from new day
			if ( ( $currenthour == 0 ) && ( $currentmin == 0 ) )
			{
				# we are supposedly in the first minute of a new day
				$newdaycounter += 1;
				if ( $newdaycounter == 10 )
				{
					# ten sentences in a row from the first minute of a new day.  Assume date rollover has occured
					$dayrollovercounter += 1;
					$dayrolloverimminent = 0;
					$newdaycounter = 0;
				}
			}
			else
			{
				$newdaycounter = 0;
			}
		}
		# end of date rollover detection

		if ( $firsttimesentenceflag == 1 )	# if first sentence in the log
		{
			$firsttime = ( ( ( ( $currenthour * 60 ) + $currentmin ) * 60 ) + $currentsec );
			$firsttimesentenceflag = 0;
			print PLOTFILE "$sentencecounter, 0\n";
		}
		else	# not first sentence in log
		{
			# compare and put in PLOTFILE
			$minssincestart = ( ( ( ( ( ( ( ( $dayrollover * 24 ) + $currenthour ) * 60 ) + $currentmin ) * 60 ) + $currentsec ) - $firsttime ) / 60 );
			print PLOTFILE "$sentencecounter, $minssincestart\n";
			$slope = $minssincestart - $lastminssincestart;
			print DERIVATIVEPLOTFILE "$sentencecounter, $slope\n";

			# compare against last time
			$deltatime = ( ( ( ( ( ( $dayrollover * 24 ) + $currenthour - $lasthour ) * 60 ) + $currentmin - $lastmin ) * 60 ) + $currentsec - $lastsec );
			if ( $deltatime > 2 )	# "large" time change
			{
				$largetimechangecounter += 1;
				# print out previous and current times
				print LARGETIMECHANGEREPORTFILE $lastsentence;
				print LARGETIMECHANGEREPORTFILE "$currentsentence\n";
			}
			elsif ( $deltatime < 0 )	# time went backwards
			{
				$reversetimecounter += 1;
				# store in array for later usage in plotting
				$backwardstimearray[$backwardstimeindex] = $sentencecounter;
				$backwardstimearray[$backwardstimeindex + 1] = $minssincestart;
				$backwardstimeindex += 2;

				# check for already going negative and printing result
				if ( $printnexttwolines == 1 )
				{
					$twolinecounter = 2;
					# don't print current or last ones, they're already printed
				}
				else
				{
					print REVERSETIMEREPORTFILE $beforelastsentence;
					print REVERSETIMEREPORTFILE $lastsentence;
					print REVERSETIMEREPORTFILE $currentsentence;
					$printnexttwolines = 1;
					$twolinecounter = 2;
				}
			}
		}
		$lasthour = $currenthour;
	        $lastmin = $currentmin;
        	$lastsec = $currentsec;
	        $beforelastsentence = $lastsentence;
        	$lastsentence = $currentsentence;
		$lastminssincestart = $minssincestart;
	}
	else	# invalid sentence
	{
		print UNEXPECTEDOUTPUTREPORTFILE;
		$invalidcounter += 1;
	}
}	# end while
}	# end if

# put second set of points to plot into PLOTFILE
# derivative (slope) of original curve
close(DERIVATIVEPLOTFILE);
print PLOTFILE "\n\n";
open(TEMP, "gpsReport.dplot") or die "Can't open file gpsReport.dplot!\n";
while ( <TEMP> )
{
	print PLOTFILE;
}
close(TEMP);
unlink("gpsReport.dplot") or die "Can't delete file gpsReport.dplot!\n";

# put third set of points to plot into PLOTFILE
# backwards time points
print PLOTFILE "\n\n";
$i = 0;
while ( $i < $backwardstimeindex )
{
	print PLOTFILE "$backwardstimearray[$i], $backwardstimearray[$i+1]\n";
	$i += 2;
}


# print the report file
print REPORTFILE "***************************\n";
print REPORTFILE "**    GPS Report File    **\n";
print REPORTFILE "***************************\n\n";
print REPORTFILE "Valid Sentences:  $sentencecounter\n";
print REPORTFILE "Errors:  $errorcounter\n";
print REPORTFILE "Invalid Data Items (not sentences or errors):  $invalidcounter\n\n";
print REPORTFILE "Time change delays:  $timechangedelay\n";
print REPORTFILE "Time change attempts:  $timechangeattempt\n";
print REPORTFILE "Time went backwards $reversetimecounter times.\n";
print REPORTFILE "GPS time was set forward by more than 2 seconds $largetimechangecounter times.\n";
print REPORTFILE "Date rollovers in this log:  $dayrollover\n\n";
if ( $ppserror == 1 )
{
	print REPORTFILE "Error:  more than one PPS success/failure message\n";
}
elsif ( $ppssuccess == -1 )
{
	print REPORTFILE "No PPS success/failure message.\n";
}
elsif ( $ppssuccess == 1 )
{
	print REPORTFILE "PPS successfully initialized.\n";
}
elsif ( $ppssuccess == 0 )
{
	print REPORTFILE "PPS initialization failed.\n";
}
else
{
	print REPORTFILE "gpsReport PPS initialization detection error!\n";
}
print REPORTFILE "NMEA Parser Errors:  $nmeaparsererror\n";
print REPORTFILE "Missing Checksums:  $missingcheck\n";
print REPORTFILE "Bad NMEA Checksums:  $badnmeacheck\n";
print REPORTFILE "Bad Checksum Field errors:  $badcheckfield\n";
print REPORTFILE "Didn't read sentence in time errors:  $nosentenceintime\n";
print REPORTFILE "Prematurely detected new sentence errors:  $prematuresentence\n";
print REPORTFILE "Maximum sentence length exceeded errors:  $longsentence\n";
print REPORTFILE "Serial port timeouts:  $serialporttimeout\n";
print REPORTFILE "Serial port read/write errors:  $serialportrwerror\n";
print REPORTFILE "Set time of day errors:  $settimeofdayerror\n";
print REPORTFILE "Other (unexpected) errors:  $strangeerror\n";

close(GPSFILE);
close(ERRORFILE);
close(REPORTFILE);
close(LARGETIMECHANGEREPORTFILE);
close(REVERSETIMEREPORTFILE);
close(UNEXPECTEDOUTPUTREPORTFILE);
close(PLOTFILE);

# print basic results file to screen
open(MAINREPORT, "gpsReport.results") or die "Can't open file gps.report!\n";
while (<MAINREPORT>)
{
        print;
}
close(MAINREPORT);
