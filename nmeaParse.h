#ifndef _NMEA_PARSER
#define _NMEA_PARSER

#include "gpsPub.h"  // for GPSPosition struct definition

class NMEAParser
{
    public:
        static bool GetTimeAndPosition(const char* buffer, GPSPosition* p);

    private:
        enum SentenceType {INVALID_SENTENCE, GPRMC, GPGGA};
        
        enum FieldType
        {
            UNUSED,     // we don't use this field in our parsing (to simplify)
            TIME,       // UTC hhmmss.[fff] e.g. "170834.123" = 17:08:34.123
            DATE,       // Date ddmmyy
            STATUS,     // "A" (active) or "V" (void)
            LAT_VAL,    // ddmm.mmmm, e.g. "4124.8963" = 41 deg 24.8963 min
            LAT_REF,    // "N" or "S"
            LON_VAL,    // dddmm.mmm, e.g. "08151.6838" = 81 deg 51.6838 min
            LON_REF,    // "E" or "W"
            FIX_MODE,   // GPGGA, "0", "1", "2" = invalid, GPS, DPGS
            SAT_USED,   // e.g. "05" = 5 satellites
            ALT_VAL,    // altitude "280.2" = 280.2
            ALT_UNIT,   // "M" = meter
            HDOP,
            GEO,
            G_UNIT,
            D_AGE,
            D_REF,
            SPD,
            HDG,
            MAG_VAR,
            MAG_REF,
            END		// not a real field, used to mark end of templates
        };   
        
        enum Status {INVALID_STATUS, ACTIVE, VOID};
        
        enum UnitType {INVALID_UNIT, METERS};  // supported altitude unit types
        
        // Templates for sentence types supported by this parser
        static const FieldType GPRMC_TEMPLATE[];
        static const FieldType GPGGA_TEMPLATE[];   
};  // end class NMEAParser

#endif  // _NMEA_PARSER
