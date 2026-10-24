//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_romdir.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     ROM File System Directory Support
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
#include "sem_romfs.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomDirectory::Name
//
// ACCESS:         public
//
// DESCRIPTION:    Return the name of the current item in the directory
//                 object.
//
// RETURNS:        A pointer to the name that is valid as long as this
//                 object is valid.
//
// REMARKS:        
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 26, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
const char *HttpdRomDirectory::Name()
{
  return (mpName);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomDirectory::Next
//
// ACCESS:         public
//
// DESCRIPTION:    Load the next directory element.
//
// RETURNS:        OS Error code
//
// REMARKS:        
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 26, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdRomDirectory::Next()
{
  unsigned char  length;
  int            rv;

  // Do we have any more entries?
  if (mCount == 0)
    return (HttpdOpSys::ERR_NOTFOUND);
  mCount--;

  // Yup. Read the length.
  rv = mpDataSource->ReadAt(&length, sizeof(length), mOffset++);
  if (rv != 0)
    return (rv);

  // Prepare the buffer.
  HttpdOpSys::Free(mpName);
  mpName = (char *)HttpdOpSys::Malloc(length + 1);
  if (mpName == NULL)
    return (HttpdOpSys::ERR_OUTOFMEM);
  mpName[length] = '\0';

  // Read the name.
  rv = mpDataSource->ReadAt(mpName, length, mOffset);
  mOffset += length;
  
  return (rv);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomDirectory::Close
//
// ACCESS:         public
//
// DESCRIPTION:    Destroy the HttpdDirectory object.
//
// REMARKS:        This method begins the destruction process. After
//                 this method is called, clients may no longer use
//                 the directory object.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 26, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdRomDirectory::Close()
{
  HttpdOpSys::Free(mpName);
  delete this;
}
