//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_rawrf.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Raw ("Stored") ROM File System Encoding
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
//  002 |03/23/02| Use correct position when mapping data.  | MYG  | M1014
// -----+--------+------------------------------------------+------+---------
//  003 |06/24/02| Really use the correct position.         | MYG  | M1022
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include "sem_romfs.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdRawRomFile::HttpdRawRomFile
//
// ACCESS:         protected
//
// DESCRIPTION:    Construct a file object for encoding 0.
//
// REMARKS:        There is little to do when constructing a raw
//                 variant of the ROM File.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
HttpdRawRomFile::HttpdRawRomFile(HttpdDataSource *p_source,
                                 unsigned long    offs,
                                 unsigned long    len)
  : mpDataSource(p_source),
    mOffset(offs),
    mLength(len),
    mCursor(0)
{}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRawRomFile::Read
//
// ACCESS:         public
//
// DESCRIPTION:    Read bytes from a HttpdRawRomFile.
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        The current cursor is updated as a side effect 
//                 of the read.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdRawRomFile::Read(void *p_buffer, size_t &sz)
{
   unsigned long remaining_length = mLength - mCursor;
   size_t        rsz;
   int           rv;

   // Step 1: EOF check.
   if (remaining_length == 0)
   {
      sz = 0;
      return (0);
   }

   // Step 2: Check length.
   if (sz > remaining_length)
     rsz = remaining_length;
   else
     rsz = sz;

   // Step 3: Read data into buffer.
   rv = mpDataSource->ReadAt(p_buffer, rsz, mOffset + mCursor);
   if (rv == 0)
   {
     sz       = rsz;
     mCursor += rsz;

     return (0);
   }
   else
     return (rv);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRawRomFile::Seek
//
// ACCESS:         public
//
// DESCRIPTION:    Adjust cursor position for a RawRomFile.
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        This is a simple operation when compared to a 
//                 compressed encoding.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdRawRomFile::Seek(long offset, int whence)
{
   switch (whence)
   {
     case FILE_SEEK_CUR:
       offset += mCursor;
       goto seek_start;

     case FILE_SEEK_END:
       offset += mLength;
       // Fallthrough:

     case FILE_SEEK_START:
     seek_start:
       if (offset < 0)
         return (HttpdOpSys::ERR_BADPARAM);
       else if (((unsigned long )offset) > mLength)
         return (HttpdOpSys::ERR_BADPARAM);
       else
         mCursor = (unsigned long )offset;
       return (0);
  
     default:
       assert("Invalid seek argument." == NULL);
       return (HttpdOpSys::ERR_BADPARAM);
   }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRawRomFile::PushToSink
//
// ACCESS:         public
//
// DESCRIPTION:    Optimized version of PushToSink.
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        Assuming the data source is mappable this provides
//                 a big performance win.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  Mar 23, 2002  BY: MYG (M1014: Use correct position)
//                 Jun 23, 2002      MYG (M1022: Really correct pos now)
//
//--------------------------------------------------------------------------
int HttpdRawRomFile::PushToSink(HttpdWritable *p_sink)
{
   unsigned long   remaining_length = mLength - mCursor;
   const void     *p_data; 

   // Step 1: EOF check.
   if (remaining_length == 0)
     return (0);

   // Step 2: Can we do a direct transfer?
   p_data = mpDataSource->AddressOf(mOffset + mCursor, remaining_length);
   if (p_data != NULL)
   {
     int rv = p_sink->Write(remaining_length, p_data);
     mpDataSource->ReleaseAddress(p_data);
     return (rv);
   }

   // Step 3: Nope, try it the old-fasioned way. 
   return (HttpdFile::PushToSink(p_sink));
}
