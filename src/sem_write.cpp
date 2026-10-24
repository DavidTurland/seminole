//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_write.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     HttpdWritable common methods.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan  14, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/14/02| File creation.                           | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdWritable::Printf
//
// ACCESS:         public
//
// DESCRIPTION:    Sink formatted print.
//
// RETURNS:        OS error code
//
// REMARKS:        This routine is a dumbed-down printf() style routine
//                 designed to make sending HTTP response messages
//                 easier.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Oct 23, 2001  BY: MYG (%t -> unsigned long)
//                 Jan 14, 2002      MYG (A1001: Abstract interface)
//
//--------------------------------------------------------------------------
int HttpdWritable::Printf(const char *p_format, ...)
{
  va_list  va;
  char     ibuf[32], *p_arg;
  int      rv;

  va_start(va, p_format);
 
  while (*p_format != '\0')
  {
    if (*p_format != '%')
    {
      rv = Write(sizeof(char), (void *)p_format);
      if (rv != 0)
        return (rv);
    }
    else
      switch (*++p_format)
      {
        case 'f': // Filler
        {
           size_t sz = va_arg(va, size_t);
           char   ch = va_arg(va, int);

           while (sz--)
           {
              rv = Write(sizeof(char), &ch);
              if (rv != 0)
                return (rv);
           }
           break;
        }

        case 'd':
          sprintf(ibuf, "%d", va_arg(va, int));
          p_arg = ibuf;
          goto str_write;

        case 't':
          sprintf(ibuf, "%lu", va_arg(va, unsigned long));
          p_arg = ibuf;
          goto str_write;

        case 's':
          p_arg = va_arg(va, char *);
str_write:
          rv = Write(strlen(p_arg), p_arg);
          if (rv != 0)
            return (rv);
          break;

        case '%':
          rv = Write(sizeof(char), "%");
          if (rv != 0)
            return (rv);
          break;
      }

    p_format++;
  }

  va_end(va);
  return (0);
}
