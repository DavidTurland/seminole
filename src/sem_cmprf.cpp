//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_cmprf.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     ROM File System Compressed File Support
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan 29, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/29/02| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |03/23/02| Cache mpUncompressed buffer for          | MYG  |
//      |        | performance.                             |      |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include "sem_romfs.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdCompressedRomFile::~HttpdCompressedRomFile
//
// ACCESS:         public
//
// DESCRIPTION:    Destroy the compressed ROM file object.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 29, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdCompressedRomFile::~HttpdCompressedRomFile()
{
  if (mpUncompressed != NULL)
    HttpdOpSys::Free(mpUncompressed);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCompressedRomFile::NextChunk
//
// ACCESS:         protected
//
// DESCRIPTION:    Read in the next chunk of the compressed file.
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        The HttpdCompressedRomFile object always keeps
//                 track of the next pending chunk. This routine
//                 obtains the address (in the datasource) of the 
//                 next chunk and then updates the internal state
//                 to indicate that the next chunk is now the current
//                 one.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 29, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdCompressedRomFile::NextChunk(unsigned long &data_addr)
{
   int            rv;

   data_addr = mNextChunk;

   // If the address of the next chunk is invalid, the package is
   // corrupt.
   if (((data_addr - mOffset) + (HTTPD_USHRT_SZ * 2)) > mDataLength)
     return (HttpdOpSys::ERR_BADFORMAT);

   // Read compressed length.
   rv = mpDataSource->ReadAt(mChunkSpace, data_addr);
   if (rv != 0)
     return (rv);
   data_addr += HTTPD_USHRT_SZ;

   // Read uncompressed length.
   rv = mpDataSource->ReadAt(mChunkSize, data_addr);
   if (rv != 0)
     return (rv);
   data_addr += HTTPD_USHRT_SZ;

   // Move to next chunk.
   mNextChunk   = data_addr + mChunkSpace;
   mChunkOffset = 0;

   // Check to make sure reading the chunk will not skip past the data
   // length.
   if ((mNextChunk - mOffset) > mDataLength)
     return (HttpdOpSys::ERR_BADFORMAT);

   return (0);   
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCompressedRomFile::DecompressChunk
//
// ACCESS:         protected
//
// DESCRIPTION:    Decompress the chunk at the specified offset
//                 into a fresh mpUncompressed buffer. 
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        If the data source can be mapped into memory
//                 the decompression is directly done from that
//                 address space. 
//
//                 Otherwise a temporary buffer is allocated to
//                 hold the compressed chunk. 
//
//                 The actual decompression routine is abstract
//                 and can safely assume that all buffers are valid
//                 for the specified lengths.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 29, 2002
// LAST MODIFIED:  Mar 23, 2002  BY: MYG (Cache buffer for performance)
//
//--------------------------------------------------------------------------
int HttpdCompressedRomFile::DecompressChunk(unsigned long data_addr)
{
   // Step 1: Prep output buffer.
   //
   // We keep track of the previous buffer size and do not re-allocate
   // the uncompressed buffer if there is enough space.
   if ((mpUncompressed == NULL) || (mBufferSize < mChunkSize))
   {
     HttpdOpSys::Free(mpUncompressed);

     mpUncompressed = (unsigned char *)HttpdOpSys::Malloc(mChunkSize);
     if (mpUncompressed == NULL)
       return (HttpdOpSys::ERR_OUTOFMEM);

     mBufferSize = mChunkSize;
   }

   const unsigned char *p_mapped = 
     (const unsigned char *)mpDataSource->AddressOf(data_addr, mChunkSpace);
   if (p_mapped)
   {
     Decompress(p_mapped);
     mpDataSource->ReleaseAddress(p_mapped);
     return (0);
   }
   else
   {
     // Well, the data source was non-mappable. We have to allocate
     // an incoming buffer to handle it. Hopefully, with mapped flash
     // chips this will be infrequent.
     int            rv;
     unsigned char *p_buffer = 
           (unsigned char *)HttpdOpSys::Malloc(mChunkSpace);

     if (p_buffer != NULL)
     {
       rv = mpDataSource->ReadAt(p_buffer, mChunkSpace, data_addr);
       if (rv == 0)
         Decompress(p_buffer);

       HttpdOpSys::Free(p_buffer);
     }
     else
       rv = HttpdOpSys::ERR_OUTOFMEM;      

     return (rv);
   }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCompressedRomFile::Read
//
// ACCESS:         public
//
// DESCRIPTION:    Read bytes from a HttpdCompressedRomFile.
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        The current cursor is updated as a side effect 
//                 of the read.
//
//                 This routine may have to load several chunks and
//                 decompress them as necessary. Therefore it can fail
//                 with a variety of errors like ERR_OUTOFMEM which is
//                 normally never returned on a call like this. 
//
//                 Make sure the code that calls this handles errors
//                 appropriately.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 29, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdCompressedRomFile::Read(void *p_buffer, size_t &sz)
{
   unsigned long  remaining_length = mContentLength - mCursor;
   unsigned long  data_addr;
   size_t         rsz, cnt;
   int            rv;
   unsigned char *p_out = (unsigned char *)p_buffer;

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

   // Step 3: Data transfer.
   cnt = rsz;
   for(;;)
   {
      // Step 3a: Compute the transfer size.
      size_t xfer_size = mChunkSize - mChunkOffset;
      if (xfer_size > cnt)
        xfer_size = cnt;

      // Step 3b: Copy from the buffer.
      if ((xfer_size != 0) && (mpUncompressed != NULL))
        memcpy(p_out, 
               mpUncompressed + mChunkOffset,
               xfer_size);

      mChunkOffset += xfer_size;
      p_out        += xfer_size;    
      cnt          -= xfer_size;      

      // Step 3c: See if we are done.
      if (cnt == 0)
        break;

      // Step 3d: Refill the chunk if necessary.
      rv = NextChunk(data_addr);
      if (rv != 0)
        return (rv);
      rv = DecompressChunk(data_addr);
      if (rv != 0)
        return (rv);
   }

   mCursor += (sz = rsz);
   return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCompressedRomFile::Seek
//
// ACCESS:         public
//
// DESCRIPTION:    Adjust cursor position for a CompressedRomFile.
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        This is a complex operation. We first attempt to
//                 identify cases that we can optimize out any 
//                 decompression or chunk loading at all (seeks relative
//                 to within the current chunk).
//
//                 If that fails we attempt to optimize away scanning
//                 chunks from where we currently are. 
//
//                 The actual approach used here is to simply scan the
//                 chunks from the beginning of the file. Since each chunk
//                 has both a compressed and uncompressed size we can scan
//                 forward without having to decompress the chunks.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 29, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdCompressedRomFile::Seek(long offset, int whence)
{
   int  rv;

   switch (whence)
   {
     case FILE_SEEK_CUR:
       offset += mCursor;
       goto seek_start;

     case FILE_SEEK_END:
       offset += mCursor;
       // Fallthrough:

     case FILE_SEEK_START:
     seek_start:
       if (offset < 0)
         return (HttpdOpSys::ERR_BADPARAM);
       else if (((unsigned long )offset) > mContentLength)
         return (HttpdOpSys::ERR_BADPARAM);
       break;
  
     default:
       assert("Invalid seek argument." == NULL);
       return (HttpdOpSys::ERR_BADPARAM);
   }

   unsigned long new_cursor = (unsigned long )offset;
   unsigned long data_addr;

   // Optimization: Can we just adjust a pointer? 
   if (new_cursor > mCursor) 
   {
     unsigned long difference = new_cursor - mCursor;

     if (difference < (mChunkSize + mChunkOffset))
     {
        mChunkOffset += difference;
        mCursor      += difference;
        return (0);
     }
     else // Align to beginning of current chunk for walk.
        mCursor      -= mChunkOffset;
   } 
   else
   {
      // In this case, we must start from the beginning of the file
      // and linearly walk down the file until we find the correct
      // chunk.
      mCursor    = 0;
      mNextChunk = mOffset;
   }
  
   // Linearly walk the chunk list.
   for(;;)
   {
      rv = NextChunk(data_addr);
      if (rv != 0)
        return (rv);

      if ((mChunkSize + mCursor) > new_cursor)
        break;

      mCursor += mChunkSize;    
   }

   rv = DecompressChunk(data_addr);
   if (rv != 0)
     return (rv);

   mChunkOffset = new_cursor - mCursor;
   mCursor      = new_cursor;

   return (0);
}


