//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_memdsrc.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Memory Data Source
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
// NAME:           HttpdMemoryDataSource::ReadAt
//
// ACCESS:         public
//
// DESCRIPTION:    Read data from a memory source.
//
// RETURNS:        OS Error code
//
// REMARKS:        For the case of the in-memory data source, this is
//                 simply a memcpy() wrapper.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 22, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int  HttpdMemoryDataSource::ReadAt(void           *p_data,
                                   size_t          sz,
                                   unsigned long   offset)
{
  if ((offset + sz) < mSize)
  {
    memcpy(p_data, mpData + offset, sz);
    return (0);
  }
  else
    return (HttpdOpSys::ERR_IO);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdMemoryDataSource::AddressOf
//
// ACCESS:         public
//
// DESCRIPTION:    Obtain the mapped address for the region.
//
// RETURNS:        A pointer to the mapped region if available, 
//                 otherwise NULL.
//
// REMARKS:        If the returned pointer is not NULL it is valid
//                 for the life of the data source.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 22, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
const void *HttpdMemoryDataSource::AddressOf(unsigned long offset, size_t sz)
{
  if ((offset + sz) < mSize)
    return ((const void *)(mpData + offset));
  else
    return (NULL);
}

