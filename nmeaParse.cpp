
#include "nmeaParse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Templates for sentence types supported by this parser
const NMEAParser::FieldType NMEAParser::GPRMC_TEMPLATE[] = 
{
    TIME,
    STATUS,
    LAT_VAL,
    LAT_REF,
    LON_VAL,
    LON_REF,
    SPD,
    HDG,
    DATE,
    MAG_VAR,
    MAG_REF,
    END  // end of template
};

const NMEAParser::FieldType NMEAParser::GPGGA_TEMPLATE[] = 
{
    TIME,
    LAT_VAL,
    LAT_REF,
    LON_VAL,
    LON_REF,
    FIX_MODE,
    SAT_USED,
    HDOP,
    ALT_VAL,
    ALT_UNIT,
    GEO,
    G_UNIT,
    D_AGE,
    D_REF,
    END  // end of template
};   

// Note: Since GPGGA sentences don't have a "DATE" field
// we don't set the "tvalid" to true even though the 
// time is there.

// This function has parsing of sentences built in ... it could be broken 
// apart to parse a sentence into information content ... i.e. a
// "NMEAMessage" class instance could be filled in from a sentence
// buffer ... and then time and position could be pulled from the
// message (sentence) information content ... but the following
// will do for now.

bool NMEAParser::GetTimeAndPosition(const char* buffer, GPSPosition* p)
{    
    // 1) Copy buffer
    unsigned int len = strlen(buffer);
    if (len > 80) return false;  // NMEA sentences are 80 chars max
    char* buf = new char[len+1];
    if (!buf)
    {
        perror("NMEAParser::GetTimeAndPosition() Error allocating buffer");
        return false;
    }
    strncpy(buf, buffer, len);
    buf[len] = '\0';
    
    // 2) Determine sentence type
    char* ptr = strchr(buf, ',');
    if (ptr) *ptr++ = '\0';
    
    SentenceType sentenceType = INVALID_SENTENCE;
    const FieldType* sentenceTemplate = NULL;
    
    if (!strcmp("GPRMC", buf))
    {
        //fprintf(stderr, "%s\n", buffer);
        sentenceType = GPRMC;
        sentenceTemplate = GPRMC_TEMPLATE;
    }
    else if (!strcmp("GPGGA", buf))
    {
        //fprintf(stderr, "%s\n", buffer);
        sentenceType = GPGGA;
        sentenceTemplate = GPGGA_TEMPLATE;
    }
    else
    {
        //fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
        //                "Unknown sentence \"%s\"\n", buf);
        delete[] buf;
        return false;
    }
            
    
    // 3) Parse sentence based on "sentenceTemplate" 
    
    // Values collected from sentence
    unsigned int hour, minute, day, month, year;
    double second, latVal, lonVal, altVal;
    double latRef = 0.0;
    double lonRef = 0.0;
    Status status = INVALID_STATUS;
    UnitType altUnit = INVALID_UNIT;
    
    // Checks
    bool gotTime = false;
    bool gotDate = false;
    bool gotLatVal = false;
    bool gotLonVal = false;
    bool gotAltVal = false;
    
    unsigned int i = 0;  // Start at beginning of the template
    FieldType fieldType = sentenceTemplate[i++];
    while (END != fieldType)
    {
        if (!ptr)
        {
            fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                            "Reached sentence end prematurely!\n");
            delete[] buf;
            return false;
        }
        char* field = ptr;
        ptr = strchr(ptr, ',');
        if (ptr) *ptr++ = '\0';
        switch (fieldType)
        {
            case TIME:
            {
                // UTC time format: hhmmss[.fff]
                if ('\0' == field[0]) break; // no TIME provided
                if (strlen(field) < 6)
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad TIME field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                char temp[3];
                temp[2] = '\0';
                // hours
                strncpy(temp, &field[0], 2);
                hour = atoi(temp);
                // minutes
                strncpy(temp, &field[2], 2);
                minute = atoi(temp);
                // seconds
                float sec;
                if (1 != sscanf(&field[4], "%f", &sec))
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad TIME (secs) field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                else
                {
                    second = (double)sec;
                    gotTime = true;
                }
            }
            break;
            
            case DATE:
            {
                // Date format: ddmmyy
                // UTC time format: hhmmss[.fff]
                if ('\0' == field[0]) break; // no DATE provided
                if (strlen(field) < 6)
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad DATE field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                char temp[3];
                temp[2] = '\0';
                // day
                strncpy(temp, &field[0], 2);
                day = atoi(temp);
                // month
                strncpy(temp, &field[2], 2);
                month = atoi(temp);
                // year
                strncpy(temp, &field[4], 2);
                year = atoi(temp);
                gotDate = true;
            }
            break;
            
            case LAT_VAL:
            {
                // Lat format: ddmm.mmmmm
                if ('\0' == field[0]) break; // no LAT_VAL provided
                if (strlen(field) < 4)
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad LAT_VAL field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                char temp[3];
                temp[2] = '\0';
                // degrees
                strncpy(temp, &field[0], 2);
                int deg = atoi(temp);
                // minutes
                float min;
                if (1 != sscanf(&field[2], "%f", &min))
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad LAT_VAL (minutes) field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                else
                {
                    latVal = (double)deg + (min/60.0);
                    gotLatVal = true;
                }
            }
            break;
            
            case LAT_REF:
                if ('\0' == field[0]) 
                    break; // no LAT_REF provided
                else if ('N' == field[0])
                    latRef = 1.0;
                else if ('S' == field[0])
                    latRef = -1.0;
                else
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad LAT_REF field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                break;
            
            case LON_VAL:
            {
                // Lon format: dddmm.mmmmm
                if ('\0' == field[0]) break; // no LON_VAL provided
                if (strlen(field) < 4)
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad LON_VAL field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                char temp[4];
                temp[3] = '\0';
                // degrees
                strncpy(temp, &field[0], 3);
                int deg = atoi(temp);
                // minutes
                float min;
                if (1 != sscanf(&field[3], "%f", &min))
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad LON_VAL (minutes) field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                else
                {
                    lonVal = (double)deg + (min/60.0);
                    gotLonVal = true;
                }
            }
            break;

            
            case LON_REF:
                if ('\0' == field[0]) 
                    break; // no LON_REF provided
                else if ('E' == field[0])
                    lonRef = 1.0;
                else if ('W' == field[0])
                    lonRef = -1.0;
                else
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad LON_REF field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                break;

            case ALT_VAL:
                // altitude value
                float alt;
                if ('\0' == field[0]) break; // no ALT_VAL provided
                if (1 != sscanf(&field[0], "%f", &alt))
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad ALT_VAL (minutes) field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                else
                {
                    altVal = (double) alt;
                    gotAltVal = true;
                }                
                break;
                
            case ALT_UNIT:
                if ('\0' == field[0]) 
                {
                    break; // no ALT_UNIT provided
                }
                else if ('M' == field[0])
                {
                    altUnit = METERS;
                }
                else  // (TBD) add support for other units
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad ALT_UNIT field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                break;
                
            case STATUS:
                if ('\0' == field[0]) 
                    break; // no STATUS provided
                else if ('A' == field[0])
                    status = ACTIVE;
                else if ('V' == field[0])
                    status = VOID;
                else
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad STATUS field in sentence!\n");
                    delete[] buf;
                    return false;
                }
                break;
            
            // GPGGA uses FIX_MODE instead of status
            // (non-zero FIX_MODE == ACTIVE status)
            case FIX_MODE:  
            {
                if ('\0' == field[0]) break; // no FIX_MODE provided
                int fixMode = atoi(field);
                if (fixMode > 0)
                {
                    status = ACTIVE;
                }
                else if (0 == fixMode)
                {
                    status = VOID;
                } 
                else
                {
                    fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                                    "Bad FIX_MODE field in sentence!\n");
                    delete[] buf;
                    return false;
                }
            }
            break;
            
            case SPD:
                break;               
                
            default:
                //fprintf(stderr, "NMEAParser::GetTimeAndPosition() "
                //                "Unknown field in sentence template?!\n");
                //delete[] buf;
                //return false;
                // Ignore "UNUSED" or other fields for now
                break;      
            
        }
        fieldType = sentenceTemplate[i++];
    }  // end while(END != fieldType)
    
    // Done with our copy of the sentence
    delete[] buf;
    
    // 4) Fill out GPSPosition struct with data collected from parsing
    if (ACTIVE == status)
    {
        // Determine GPS Time (if valid)
        if (gotTime && gotDate)
        {
            struct tm t;
            // Get epoch reference for ...
            t.tm_year=70;
            t.tm_mon=0;
            t.tm_mday=1;
            t.tm_sec=0;
            t.tm_min=0;
            t.tm_hour=0;
            t.tm_isdst=0;
            // and deterimine local machine timezone offset
            time_t offsetSecs = mktime(&t);
            // Fill in "t" for UTC time from sentence
            if (year < 70)
                t.tm_year = year + 100;
            else
            {
                fprintf(stderr, "NMEAParser::GetTimeAndPostion() error: "
                                " Invalid \"year\".\n");
                delete[] buf;
                return false;
            }
            t.tm_mon = month - 1;  // NMEA uses 1-12
            t.tm_mday = day;
            t.tm_hour = hour;
            t.tm_min = minute;
            t.tm_sec = (int) second;
            // Compute seconds since GMT epoch (using offset)
            time_t totalSecs = mktime(&t) - offsetSecs;
            // Compute microseconds
            unsigned long uSecs = (unsigned long)((second - (double)t.tm_sec)*1.0e06 + 0.5);            
            p->gps_time.tv_sec = totalSecs;
            p->gps_time.tv_usec = uSecs;
            p->tvalid = true;
        }
        else
        {
            p->tvalid = false;
        }
        if (gotLatVal && (0.0 != latRef) && gotLonVal && (0.0 != latRef))
        {
            p->x = lonRef * lonVal;
            p->y = latRef * latVal;
            p->xyvalid = true;
        }
        else
        {
            p->xyvalid = false;
        }
        if (gotAltVal && (INVALID_UNIT != altUnit))
        {
            p->z = altVal;  // always METERS for now
            p->zvalid = true;
        }
        else
        {
            p->zvalid = false;
        }
        return true;
    }
    else
    {
        //fprintf(stderr, "Sentence with VOID status ...\n");
        return false;
    }
}  // end NMEAParser::GetTimeAndPosition()
