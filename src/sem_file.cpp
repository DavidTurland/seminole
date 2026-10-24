//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_file.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     File System Support
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan 19, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/19/02| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileSystem::CommonFileInfo
//
// ACCESS:         protected
//
// DESCRIPTION:    Fill in the common fields of the HttpdFileInfo object
//                 for the filesystem.
//
// RETURNS:        OS Error code.
//
// REMARKS:        This acts as a helper that the implementations of
//                 HttpdFileSystem::FileInfo can call to do common
//                 processing.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdFileSystem::CommonFileInfo(HttpdFileInfo &finfo)
{
  finfo.FileSystem(this);

  return (0);
}


//---------------------------------------------------------------------------
//
// NAME:           HttpdFileSystem::Open
//
// ACCESS:         public
//
// DESCRIPTION:    Open a directory for iteration.
//
// RETURNS:        An OS error (always).
//
// REMARKS:        For file systems that do not support directories
//                 (such as ROM file systems), this default method
//                 will take effect.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdFileSystem::Open(const HttpdFileInfo  &info,
                          HttpdDirectory      *&p_directory)
{
  return (HttpdOpSys::ERR_WRONGTYPE);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFile::Write
//
// ACCESS:         public
//
// DESCRIPTION:    Default behavior for file writing.
//
// RETURNS:        An error code indicating that writing is not
//                 possible at this time.
//
// REMARKS:        This method can be overridden if a writable filesystem
//                 is available.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdFile::Write(void *p_buffer, size_t sz)
{
  return (HttpdOpSys::ERR_NOTREADY);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFile::PushToSink
//
// ACCESS:         public
//
// DESCRIPTION:    Transfer mechanism for filesystem to sink objects.
//
//                 This is one of the main data pumps for file
//                 delivery. The system constant HTTPD_XFER_BUF_SIZE 
//                 determines the amount of throughput this routine 
//                 will be able to deliver. 
//
// RETURNS:        0 on success, an error code on failure.
//
// REMARKS:        This method is normally used to pump data from
//                 a file into a sink. In some cases 
//                 (see HttpdDataSource::AddressOf) some subclasses
//                 of HttpdFile can use a more efficient transfer
//                 mechanism.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdFile::PushToSink(HttpdWritable *p_sink)
{
  unsigned char  xfer_buf[HTTPD_XFER_BUF_SIZE];  
  int            err_code;

  for(;;)
  {
    size_t xfer_size = sizeof(xfer_buf);

    // Read in a buffer.
    err_code = Read(xfer_buf, xfer_size);

    // Check for error.
    if (err_code != 0)
      return (err_code);

    // Check for end-of-file.
    if (xfer_size == 0)
      break;

    err_code = p_sink->Write(xfer_size, xfer_buf);
    if (err_code != 0)
      return (err_code);
  }

  return (0);
}


//---------------------------------------------------------------------------
//
// NAME:           HttpdFileInfo::~HttpdFileInfo
//
// ACCESS:         public
//
// DESCRIPTION:    Cleanup for file information.
//
// REMARKS:        It is assumed that the HttpdOpSys class is used for
//                 memory allocation by HttpdFileInfo factories.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 16, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
HttpdFileInfo::~HttpdFileInfo()
{
  if (mFreeMimeType)
    HttpdOpSys::Free((void *)mpMimeType);
      
  if (mFreeLocation)
    HttpdOpSys::Free(mLocation.mpVoid);
}

