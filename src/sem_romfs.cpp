//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_romfs.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     ROM File System Support
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
//  002 |06/23/02| Add LZARI compression.                   | MYG  | M1021
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include "sem_romfs.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFile::Close
//
// ACCESS:         public
//
// DESCRIPTION:    Close a file open from the ROM filesystem.
//
// REMARKS:        All files from the ROM filesystem are allocated
//                 from the heap using special new and delete operators.
//
//                 All that is necessary is to deallocate the object
//                 by default.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdRomFile::Close()
{
  delete this;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFile::~HttpdRomFile
//
// ACCESS:         protected
//
// DESCRIPTION:    Virtual destructor for an HttpdRomFile.
//
// REMARKS:        Subclasses can override this to provide a cleanup
//                 mechanism.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
HttpdRomFile::~HttpdRomFile()
{}

/***********************************************************************/

const unsigned long         OFFS_MAGIC      = 0;
const unsigned long         OFFS_NUM_FILES  = OFFS_MAGIC
                                            + 4;
const unsigned long         OFFS_PKG_VERS   = OFFS_NUM_FILES 
                                            + HTTPD_USHRT_SZ;
const unsigned long         OFFS_FILE_TABLE = OFFS_PKG_VERS
                                            + HTTPD_USHRT_SZ;
const unsigned long         OFFS_MIME_TABLE = OFFS_FILE_TABLE
                                            + HTTPD_ULONG_SZ;
const unsigned long         OFFS_TSTAMP     = OFFS_MIME_TABLE
                                            + HTTPD_ULONG_SZ;
const unsigned long         SIZE_TSTAMP     = 8;

const unsigned long         SIZE_FILE_NAME  = 128;
const unsigned long         SIZE_MIME_ENTRY = 64;
const unsigned long         SIZE_FILE_ENTRY = HTTPD_ULONG_SZ
                   /* Offset        */      + HTTPD_ULONG_SZ
                   /* Content size  */      + HTTPD_ULONG_SZ
                   /* Physical size */      + HTTPD_ULONG_SZ
                   /* MIME type     */      + HTTPD_USHRT_SZ
                   /* Encoding      */      + HTTPD_USHRT_SZ
                                            ;

const unsigned long         OFFS_FILE_NAME  = 0;
const unsigned long         OFFS_FILE_POS   = OFFS_FILE_NAME 
                                            + HTTPD_ULONG_SZ;
const unsigned long         OFFS_FILE_CLEN  = OFFS_FILE_POS
                                            + HTTPD_ULONG_SZ;
const unsigned long         OFFS_FILE_SIZE  = OFFS_FILE_CLEN
                                            + HTTPD_ULONG_SZ;
const unsigned long         OFFS_FILE_MIME  = OFFS_FILE_SIZE
                                            + HTTPD_ULONG_SZ;
const unsigned long         OFFS_FILE_ENC   = OFFS_FILE_MIME
                                            + HTTPD_USHRT_SZ;
const unsigned short        ENC_DIRECTORY   = 0xfff0;

/***********************************************************************/

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFileSystem::FindFile
//
// ACCESS:         protected
//
// DESCRIPTION:    Locate the TOC entry of a file in the filesystem.
//
// RETURNS:        OS Error code. On success, rpos is the offset 
//                 in the source data stream of the TOC entry.
//
// REMARKS:        The TOC is always sorted by the package generator
//                 so a binary search will suffice. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int  HttpdRomFileSystem::FindFile(const char            *p_name,
                                  unsigned long         &rpos)
{
  long  high, low;

  low  = 0;
  high = mFileCount - 1;

  // Binary search the file table for the appropriate entry.
  while (low <= high)
  {
     int           mid, res;
     char          file_name_buffer[SIZE_FILE_NAME + 1];
     unsigned long pos;
     unsigned long fnpos;
     unsigned char file_name_len;
     int           rv;
 
     mid = (low + high) / 2;
     pos = mFileTable + (mid * SIZE_FILE_ENTRY);

     // Read location of file name.
     rv = mpDataSource->ReadAt(fnpos, pos);
     if (rv != 0)
       return (rv);

     // Read length of file name.
     rv = mpDataSource->ReadAt(&file_name_len, sizeof(file_name_len), fnpos);
     if (rv != 0)
       return (rv);
     if ((file_name_len == 0) || (file_name_len > SIZE_FILE_NAME))
       return (HttpdOpSys::ERR_IO);

     // Terminate the buffer.
     file_name_buffer[file_name_len] = '\0';

     // Read the actual file name.
     rv = mpDataSource->ReadAt(file_name_buffer, 
                               file_name_len,
                               fnpos + sizeof(file_name_len));
     if (rv != 0)
       return (rv);

     // Test against key.
     res = strcmp(file_name_buffer, p_name);

     if (res < 0) // Try above
       low = mid + 1;
     else if (res > 0) // Try below
       high = mid - 1;
     else // Found it
     {
       rpos  = pos;
       return (0);
     }
  }

  return (HttpdOpSys::ERR_NOTFOUND);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFileSystem::ParseTime
//
// ACCESS:         protected
//
// DESCRIPTION:    Parse the timestamp information in the package
//                 header.
//
// RETURNS:        OS Error code. 
//
// REMARKS:        All files in the package share the same date and
//                 time. This date and time is encoded in the header
//                 such that most of the struct tm fields can be 
//                 correctly set from the packed form.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdRomFileSystem::ParseTime()
{
  char            bytes[4];
  unsigned short  year;
  unsigned short  packed_day;
  int             rv;

  rv = mpDataSource->ReadAt(bytes, sizeof(bytes), OFFS_TSTAMP);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(packed_day, OFFS_TSTAMP + sizeof(bytes));
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(year, 
                            OFFS_TSTAMP + sizeof(bytes) + sizeof(packed_day));
  if (rv != 0)
    return (rv);

  memset(&mPackageTime, 0, sizeof(mPackageTime));

  mPackageTime.tm_min  = bytes[0];
  mPackageTime.tm_hour = bytes[1];
  mPackageTime.tm_mday = bytes[2];
  mPackageTime.tm_mon  = bytes[3];
  mPackageTime.tm_year = year;
  mPackageTime.tm_wday = packed_day & 7;
  mPackageTime.tm_yday = packed_day >> 3;

  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFileSystem::Mount
//
// ACCESS:         public
//
// DESCRIPTION:    Connect a data source to the RomFileSystem
//                 engine. 
//
// RETURNS:        OS Error code. 
//
// REMARKS:        The validity of the package is analized and the
//                 data source is interpreted by the file system. 
//
//                 The HttpdRomFileSystem may not be used until
//                 it is mounted.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdRomFileSystem::Mount(HttpdDataSource *p_source)
{
  char           magic[4];
  unsigned short version;
  int            rv;

  mpDataSource = p_source;

  // Check for type signature 'SEML' (Seminole Library)
  rv = mpDataSource->ReadAt(magic, sizeof(magic), OFFS_MAGIC);
  if (rv != 0)
   return (rv);

  if ((magic[0] != 'S')
  ||  (magic[1] != 'E')
  ||  (magic[2] != 'M')
  ||  (magic[3] != 'L'))
    return (HttpdOpSys::ERR_BADFORMAT);

  // Now read the lengths and offsets. 
  rv = mpDataSource->ReadAt(mFileCount, OFFS_NUM_FILES);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(mFileTable, OFFS_FILE_TABLE);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(version, OFFS_PKG_VERS);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(mMimeTable, OFFS_MIME_TABLE);
  if (rv != 0)
    return (rv);

  // Read in the time stamp.
  rv = ParseTime();
  if (rv != 0)
    return (rv);

  // Now sanity check some things. 
  if ((mFileCount == 0) || (version != 1))
    return (HttpdOpSys::ERR_BADFORMAT);
  if (mFileTable < (OFFS_MIME_TABLE + HTTPD_ULONG_SZ))
    return (HttpdOpSys::ERR_BADFORMAT);
  if (mMimeTable < (OFFS_MIME_TABLE + HTTPD_ULONG_SZ))
    return (HttpdOpSys::ERR_BADFORMAT);

  // It looks good.
  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFileSystem::FileInfo
//
// ACCESS:         public
//
// DESCRIPTION:    Obtain information about a file object.
//
// RETURNS:        OS Error code. 
//
// REMARKS:        The TOC header is analized and packed into
//                 a file info packet.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdRomFileSystem::FileInfo(const char    *p_path, 
                                 HttpdFileInfo &info)
{
  unsigned long rpos;
  int           rv;
  
  rv = CommonFileInfo(info);
  if (rv != 0)
    return (rv);

  rv = FindFile(p_path, rpos);

  if (rv != 0)
    return (rv);
  else
  { 
    HttpdParameter   p;
    unsigned long    size;
    unsigned short   mime_offset, encoding;
    unsigned char    string_length;
    char            *p_mime_type;

    rv = mpDataSource->ReadAt(size, rpos + OFFS_FILE_CLEN);
    if (rv != 0)
      return (rv);
    rv = mpDataSource->ReadAt(encoding, rpos + OFFS_FILE_ENC);
    if (rv != 0)
      return (rv);
    rv = mpDataSource->ReadAt(mime_offset, rpos + OFFS_FILE_MIME);
    if (rv != 0)
      return (rv);
    rv = mpDataSource->ReadAt(&string_length, 
                              sizeof(string_length),  
                              mMimeTable + mime_offset);
    if (rv != 0)
      return (rv);

    p_mime_type = (char *)HttpdOpSys::Malloc(string_length + 1);
    if (p_mime_type == NULL)
      return (HttpdOpSys::ERR_OUTOFMEM);

    p_mime_type[string_length] = '\0';

    rv = mpDataSource->ReadAt(p_mime_type, 
                              string_length,
                              mMimeTable 
                            + mime_offset 
                            + sizeof(string_length));

    if (rv != 0)
    {
      HttpdOpSys::Free(p_mime_type);
      return (rv);
    }

    p.mULong = rpos;

    info.LastModificationTime(&mPackageTime);
    info.CreationTime(&mPackageTime);
    info.Location(p, false);
    info.Size(size);
    info.IsDir(encoding == ENC_DIRECTORY);
    info.MimeType((const char *)p_mime_type, true);
    
    return (0);
  }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRomFileSystem::Open
//
// ACCESS:         public
//
// DESCRIPTION:    Given file information, open a file for access.
//
// RETURNS:        OS Error code. 
//
// REMARKS:        This is the factory for the various encoding
//                 abstractions. If you wish to add a new encoding,
//                 add it here.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  Jun 23, 2002  BY: MYG (M1021: Add LZARI compression)
//
//--------------------------------------------------------------------------
int HttpdRomFileSystem::Open(const HttpdFileInfo  &info,
                             int                   mode,
                             HttpdFile           *&p_file)
{
  unsigned short  encoding; 
  unsigned long   rpos;
  unsigned long   dpos;
  unsigned long   clen;
  unsigned long   dlen;
  int             rv;

  rpos = info.Location().mULong;

  if (mode != FILE_READ_ONLY)
    return (HttpdOpSys::ERR_SYSPERM);
  rv = mpDataSource->ReadAt(encoding, rpos + OFFS_FILE_ENC);
  if  (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(dpos, rpos + OFFS_FILE_POS);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(dlen, rpos + OFFS_FILE_SIZE);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(clen, rpos + OFFS_FILE_CLEN);
  if (rv != 0)
    return (rv);

  switch (encoding)
  {
    case 0: // Encoding of type "store"
      if (clen != dlen)
        return (HttpdOpSys::ERR_IO);
      p_file = (HttpdFile *)new HttpdRawRomFile(mpDataSource,
                                                dpos,
                                                clen);
      break;

#ifdef HTTPD_INC_LZRW1KH_COMPRESSION
    case 1: // Encoding of type "lzrw1kh"
      p_file = (HttpdFile *)new HttpdLzrw1KhCompressedRomFile(mpDataSource,
                                                              dpos,
                                                              clen,
                                                              dlen);
      break;
#endif

#ifdef HTTPD_INC_LZARI_COMPRESSION
    case 2: // Encoding of type "lzari"
      p_file = (HttpdFile *)new HttpdLzAriCompressedRomFile(mpDataSource,
                                                            dpos,
                                                            clen,
                                                            dlen);
      break;
#endif

    default: 
      return (HttpdOpSys::ERR_WRONGTYPE);
  }

  if (p_file == NULL)
    return (HttpdOpSys::ERR_OUTOFMEM);

  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Open
//
// ACCESS:         public
//
// DESCRIPTION:    Open a file directory for iteration.
//
// RETURNS:        0 on success or HttpdOpSys::ERR_* on error.
//                 On success, p_out points to a valid HttpdDirectory
//                 object.
//
// REMARKS:        We support directory iteration only when the package
//                 has the provided information. Otherwise, permission
//                 is denied.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 26, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdRomFileSystem::Open(const HttpdFileInfo  &info, 
                             HttpdDirectory      *&p_directory)
{
  unsigned long  rpos;
  int            rv;
  unsigned short encoding; 
  unsigned long  dpos;
  unsigned long  clen;

  // Get the TOC entry.
  rpos = info.Location().mULong;
 
  // Now check the encoding.
  rv = mpDataSource->ReadAt(encoding, rpos + OFFS_FILE_ENC);
  if (rv != 0)
    return (rv);
  if (encoding != ENC_DIRECTORY)
    return (HttpdOpSys::ERR_WRONGTYPE);

  // Okay, now read in the parameters.
  rv = mpDataSource->ReadAt(dpos, rpos + OFFS_FILE_POS);
  if (rv != 0)
    return (rv);
  rv = mpDataSource->ReadAt(clen, rpos + OFFS_FILE_CLEN);
  if (rv != 0)
    return (rv);

  // We may have been encoded with a length of 0, in this case,
  // we can't access the directory.
  if (clen == 0)
    return (HttpdOpSys::ERR_SYSPERM);

  // If all went well, we can configure the object.
  p_directory = (HttpdDirectory *)new HttpdRomDirectory(dpos, 
                                                        clen, 
                                                        mpDataSource);

  return (0);
}

