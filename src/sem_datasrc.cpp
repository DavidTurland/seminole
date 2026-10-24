//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_datasrc.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Data Source Abstraction
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan 22, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/22/02| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdDataSource::ReadAt
//
// ACCESS:         public
//
// DESCRIPTION:    Read an unsigned long from a data source.
//
// RETURNS:        OS Error code
//
// REMARKS:        The encoding of unsigned long integers must match
//                 the encoding used in the SCPG generator
//                 program.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int  HttpdDataSource::ReadAt(unsigned long  &lval, 
                             unsigned long   offset)
{
  unsigned char buf[HTTPD_ULONG_SZ];
  unsigned int  i;
  int           rv;

  rv = ReadAt((void *)buf, sizeof(buf), offset);
  if (rv != 0)
    return (rv);

  lval = 0;
  for(i = 0; 
      i < ((HTTPD_ULONG_SZ < sizeof(lval)) ? HTTPD_ULONG_SZ : sizeof(lval)); 
      i++)
    lval |= (buf[i] << (i * CHAR_BIT));
  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdDataSource::ReadAt
//
// ACCESS:         public
//
// DESCRIPTION:    OS Error code
//
// RETURNS:        True on success, false on error.
//
// REMARKS:        The encoding of unsigned short integers must match
//                 the encoding used in the SCPG generator
//                 program.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int  HttpdDataSource::ReadAt(unsigned short  &sval, 
                             unsigned long    offset)
{
  unsigned char buf[HTTPD_USHRT_SZ];
  unsigned int  i;
  int           rv;

  rv = ReadAt((void *)buf, sizeof(buf), offset);
  if (rv != 0)
    return (rv);

  sval = 0;
  for(i = 0; 
      i < ((HTTPD_USHRT_SZ < sizeof(sval)) ? HTTPD_USHRT_SZ : sizeof(sval)); 
      i++)
    sval |= (buf[i] << (i * CHAR_BIT));
  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdDataSource::AddressOf
//
// ACCESS:         public
//
// DESCRIPTION:    Obtain the mapped address for the region if
//                 applicable.
//
// RETURNS:        A pointer to the mapped region if available, 
//                 otherwise NULL.
//
// REMARKS:        If the returned pointer is not NULL it should be
//                 released when no longer used with the ReleaseAddress
//                 method.
//
//                 For some types of data sources (flash ROM) this
//                 can be overridden to return the address of the
//                 mapped flash area.
//
//                 This function should only be implemented when it
//                 is more efficient to obtain data this way. If not,
//                 callers will take an alternate flow and use the ReadAt()
//                 methods. 
//
//                 However, in the case that this routine does not return
//                 NULL some performance optimizations can be made.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
const void *HttpdDataSource::AddressOf(unsigned long offset, size_t sz)
{
  return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdDataSource::ReleaseAddress
//
// ACCESS:         public
//
// DESCRIPTION:    Release a mapped address region.
//
// REMARKS:        This method should be implemented if the implementation
//                 of AddressOf (above) requires some cleanup. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Feb  1, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdDataSource::ReleaseAddress(const void *p_data)
{
}
