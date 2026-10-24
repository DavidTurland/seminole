//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_time.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Time (and date) processing.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct  7, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/07/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |10/08/01| Fixed a bug in Validate() that didn't    | SPC  |
//      |        | handle leap years.  Added DaysInMonth()  |      |
//      |        | call to correctly calculate the number   |      |
//      |        | of days in a year.                       |      |
// -----+--------+------------------------------------------+------+---------
//  003 |10/18/01| Keep (and ensure) everything (is) in     | MYG  |
//      |        | GMT.                                     |      |
// -----+--------+------------------------------------------+------+---------
//  004 |01/18/02| Use struct tm and not time_t.            | MYG  | M1007
// -----+--------+------------------------------------------+------+---------
//  005 |05/11/02| Adjust values from struct tm to match    | SPC  | S1003
//      |        | values stored in mYear and mMonth.       |      |
//      |        | Compare seconds as well in Compare()     |      |
// -----+--------+------------------------------------------+------+---------
//  006 |05/31/02| Optimize for space.                      | SPC  | S1004
// -----+--------+------------------------------------------+------+---------
//  007 |07/10/02| Fix parsing of dates on Wednesday.       | MYG  | M1028
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

const unsigned int HTTPD_NUM_MONTHS = 12;

static const char Gmt[] = "GMT";

const char HttpdTimeStamp::mMonthTable[] = "Jan\0Feb\0Mar\0Apr\0May\0Jun\0"
                                           "Jul\0Aug\0Sep\0Oct\0Nov\0Dec" ;
const char HttpdTimeStamp::mDayTable[] = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";

const unsigned char HttpdTimeStamp::mDaysInMonth[HTTPD_NUM_MONTHS] =
{
   31, 28, 31, 30, 31, 30, 
   31, 31, 30, 31, 30, 31 
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::FindString
//
// ACCESS:         private
//
// DESCRIPTION:    Find the specified string in the table.
//
// RETURNS:        The 1-based index of the string (if found).
//                 0 is returned on failure.
//
// REMARKS:        This function is used to search for both
//                 days and months. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  May 31, 2002  BY:  SPC  (S1004: Space optimizations)
//
//--------------------------------------------------------------------------
unsigned int HttpdTimeStamp::FindString
  ( // PARAMETERS:
    const char         *p_table,
    const unsigned int  table_size,
    const char         *p_str
  )
{
  unsigned int  idx;

  assert(p_table != NULL);
  assert(p_str   != NULL);

  for(idx = 1; idx <= table_size; idx++, p_table += 4)
    if (strcmp(p_table, p_str) == 0) 
      return (idx);

  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::ParseTime
//
// ACCESS:         private
//
// DESCRIPTION:    Parse the time in HH:MM:SS format.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        This function modifies the contents of p_time to
//                 perform the parse. In addition, validation is not
//                 done so it is still necessary to call Validate().
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::ParseTime(char *p_time)
{
   assert(p_time != NULL);
   
   // Make sure the string is the exact length.
   if (strlen(p_time) != 8)
     return (false);
     
   // Remove the ':' characters from the time. 
   p_time[2] = '\0';
   p_time[5] = '\0';  

   // Compute the time component.
   mHour   = (unsigned int )strtoul(p_time,     NULL, 10);
   mMinute = (unsigned int )strtoul(p_time + 3, NULL, 10);
   mSecond = (unsigned int )strtoul(p_time + 6, NULL, 10);

   return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::ParseRfc822
//
// ACCESS:         private
//
// DESCRIPTION:    Parse the the time in RFC 822 format.
//
// RETURNS:        True on success (valid time), false on failure.
//
// REMARKS:        An RFC 822 date is of the following format:
//
//                    Sun, 06 Nov 1994 08:49:37 GMT
//
//                 This function modifies the contents of p_rest
//                 during the parse. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  Oct 18, 2001  BY:  MYG (Check for GMT)
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::ParseRfc822(char *p_rest)
{
   char *p_mday = HttpdUtilities::StrChop(p_rest);
   char *p_mon  = HttpdUtilities::StrChop(p_rest);
   char *p_year = HttpdUtilities::StrChop(p_rest);
   char *p_time = HttpdUtilities::StrChop(p_rest);
   char *p_gmt  = HttpdUtilities::StrChop(p_rest);
   
   if ((p_mday == NULL) 
   ||  (p_mon  == NULL) 
   ||  (p_year == NULL) 
   ||  (p_time == NULL)
   ||  (p_gmt  == NULL))
     return (false);
    
   if (strcmp(Gmt, p_gmt) != 0)
     return (false);

   mDay   = (unsigned int )strtoul(p_mday, NULL, 10);
   mMonth = FindString(mMonthTable, 12, p_mon);
   mYear  = (unsigned int )strtoul(p_year, NULL, 10);

   if (!ParseTime(p_time))
     return (false);
   
   return (Validate());
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::ParseRfc850
//
// ACCESS:         private
//
// DESCRIPTION:    Parse the the time in RFC 850 format.
//
// RETURNS:        True on success (valid time), false on failure.
//
// REMARKS:        An RFC 850 date is of the following format:
//
//                    Sunday, 06-Nov-94 08:49:37 GMT
//
//                 This function modifies the contents of p_rest
//                 during the parse. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  Oct 18, 2001  BY:  MYG (Check for GMT)
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::ParseRfc850(char *p_rest)
{
   char *p_date  = HttpdUtilities::StrChop(p_rest);
   char *p_time  = HttpdUtilities::StrChop(p_rest);
   char *p_gmt   = HttpdUtilities::StrChop(p_rest);

   if ((p_date == NULL) 
   ||  (p_time == NULL)
   ||  (p_gmt  == NULL))
     return (false);

   if (strcmp(Gmt, p_gmt) != 0)
     return (false);

   if (strlen(p_date) != 9)
     return (false);

   p_date[2] = '\0';
   p_date[6] = '\0';

   mDay    = (unsigned int )strtoul(p_date,     NULL, 10);
   mMonth  = FindString(mMonthTable, 12, p_date + 3);
   mYear   = (unsigned int )strtoul(p_date + 7, NULL, 10);
   mYear  += (mYear > 65) ? 1900 : 2000;

   if (!ParseTime(p_time))
     return (false);

   return (Validate());
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::ParseAscTime
//
// ACCESS:         private
//
// DESCRIPTION:    Parse the the time in the ANSI C asctime() format.
//
// RETURNS:        True on success (valid time), false on failure.
//
// REMARKS:        The asctime() format is:
//
//                    Sun Nov  6 08:49:37 1994
//
//                 This function modifies the contents of p_rest
//                 during the parse. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::ParseAscTime(char *p_rest)
{
   char *p_wkday = HttpdUtilities::StrChop(p_rest);
   char *p_mon   = HttpdUtilities::StrChop(p_rest);
   char *p_mday  = HttpdUtilities::StrChop(p_rest);
   char *p_time  = HttpdUtilities::StrChop(p_rest);
   char *p_year  = HttpdUtilities::StrChop(p_rest);
   
   if ((p_wkday == NULL)
   ||  (p_mon   == NULL)
   ||  (p_mday  == NULL)
   ||  (p_time  == NULL)
   ||  (p_year  == NULL))
     return (false);

   if (FindString(mDayTable, 7, p_wkday) == 0)
     return (false);

   mDay    = (unsigned int )strtoul(p_mday, NULL, 10);
   mMonth  = FindString(mMonthTable, 12, p_mon);
   mYear   = (unsigned int )strtoul(p_year, NULL, 10);

   if (!ParseTime(p_time))
     return (false);
     
   return (Validate());
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::DaysInMonth
//
// ACCESS:         protected
//
// DESCRIPTION:    Return the number of days in a month, taking into account
//                 the leap year rules.
//
// RETURNS:        The number of days in a given month.
//
// REMARKS:        I don't take into account the possible rule for years
//                 divisible by 4,000, seeing how there isn't yet a consensus,
//                 and we still have 1,999 years left before this will be a
//                 problem.
//
// AUTHOR:         S. Conner
// CREATED:        Oct  8, 2001
// LAST MODIFIED:  May 31, 2002  BY:  SPC (S1004: Optimizations)
//
//--------------------------------------------------------------------------
unsigned char HttpdTimeStamp::DaysInMonth
  ( // PARAMETERS:
    unsigned int year,
    unsigned int month
  )
{
  if (month == 2)
  {
    //---------------------------------------------------------
    // in case you didn't know, leap years are those years
    // that are divisible by 4, except if it's divisible by
    // 100, then it's not, unless it's divisible by 400, then
    // it is.  1800 and 1900 were NOT leap years, but 2000 is.
    //----------------------------------------------------------
    if (((year % 400) == 0)
    ||  (((year % 4) == 0) && ((year % 100) != 0))) 
      return (29);
  }

  return (mDaysInMonth[month - 1]);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::Validate
//
// ACCESS:         protected
//
// DESCRIPTION:    Verify that the values of the object make sense.
//
// RETURNS:        True for a valid time, false for an invalid time.
//
// REMARKS:        This function attempts to ensure that the component 
//                 parts make sense. It seems to be possible to verify 
//                 almost everything but the year. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  Oct  8, 2001  BY:  SPC (handle Leap years)
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::Validate()
{
   if ((mMonth < 1) || (mMonth > HTTPD_NUM_MONTHS))
     return (false);
     
   if ((mDay < 1) || (mDay > DaysInMonth(mYear, mDay)))
     return (false);

   if ((mHour > 23) || (mMinute > 59) || (mSecond > 59))
     return (false);
     
   return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::Parse
//
// ACCESS:         public
//
// DESCRIPTION:    Parse the timestamp.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        This function determines the format of the time string
//                 and dispatches the request to the various helper
//                 functions.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::Parse(const char *p_input)
{
  bool  result;
  char *p_timestamp;

  assert(p_timestamp != NULL);
  
  p_timestamp = HttpdUtilities::StrVCat(p_input, (char *)0);
  if (p_timestamp == NULL)
    return (false);

  char *p_comma = strchr(p_timestamp, ',');
  
  if (p_comma == NULL)
    result = ParseAscTime(p_timestamp);
  else
  {
     *p_comma++ = '\0';
     
     if (strlen(p_timestamp) > 3)
     {
       p_timestamp[3] = '\0';
       if (FindString(mDayTable, 7, p_timestamp) == 0)
         result = false;
       else
         result = ParseRfc850(p_comma);
     }
     else
     {
       if (FindString(mDayTable, 7, p_timestamp) == 0)
         result = false;
       else
         result = ParseRfc822(p_comma);
     }
  } 
  
  HttpdOpSys::Free(p_timestamp);
  return (result);   
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::Convert
//
// ACCESS:         public
//
// DESCRIPTION:    Convert our values from an ANSI C time_t.
//
// RETURNS:        True on success, false on failure.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  Jan 18, 2002  BY:  MYG (M1007: Use broken-out time)
//                 May 11, 2002       SPC (S1003: adjust values from p_tm)
//
//--------------------------------------------------------------------------
bool HttpdTimeStamp::Convert(const struct tm *p_tm)
{
  mDay     = p_tm->tm_mday;
  mMonth   = p_tm->tm_mon  + 1;
  mYear    = p_tm->tm_year + 1900;
  mHour    = p_tm->tm_hour;
  mMinute  = p_tm->tm_min;
  mSecond  = p_tm->tm_sec;

  return (Validate());
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdTimeStamp::Compare
//
// ACCESS:         public
//
// DESCRIPTION:    Compare this object (chronologically) with the
//                 object pointed to by p_to.
//
// RETURNS:        -1 if p_to is in the future relative to this.
//                  0 if p_to is identical to this. 
//                  1 if p_to is in the past relative to this.
//
// AUTHOR:         S. Conner   
// CREATED:        Oct  7, 2001
// LAST MODIFIED:  May 11, 2002  BY:  SPC (S1003: Check for seconds)
//
//--------------------------------------------------------------------------
int HttpdTimeStamp::Compare(const HttpdTimeStamp *p_to)
{
  assert(p_to != NULL);

  int   rc;

  if ((rc = (int )mYear   - (int )p_to->mYear)   != 0) 
    return (rc);
  if ((rc = (int )mMonth  - (int )p_to->mMonth)  != 0) 
    return (rc);
  if ((rc = (int )mDay    - (int )p_to->mDay)    != 0) 
    return (rc);
  if ((rc = (int )mHour   - (int )p_to->mHour)   != 0) 
    return (rc);
  if ((rc = (int )mMinute - (int )p_to->mMinute) != 0) 
    return (rc);
  if ((rc = (int )mSecond - (int )p_to->mSecond) != 0)
    return (rc);

  return (0);
}

