//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_native.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Native POSIX filesystem wrapper.
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

class HttpdNativeFileSystem : public HttpdFileSystem
{
protected:
   const char *ComputeMime(const char *p_path);

public:
   virtual int FileInfo(const char    *p_path, 
                        HttpdFileInfo  &info);
   virtual int Open(const HttpdFileInfo  &info,
                    int                   mode,
                    HttpdFile           *&p_file);
   virtual int Open(const HttpdFileInfo  &info,
                    HttpdDirectory      *&p_dir);
};

class HttpdNativeFile : public HttpdFile
{
  friend class HttpdNativeFileSystem;

  FILE  *mpFile;

protected:
  HttpdNativeFile(FILE *p_file) 
  : mpFile(p_file) 
  {}

public:
  virtual int Read(void *p_buffer, size_t &sz);
  virtual int Write(void *p_buffer, size_t sz);
  virtual int Seek(long offset, int whence);
  virtual void Close();
};

class HttpdNativeDirectory : public HttpdDirectory
{
  friend class HttpdNativeFileSystem;

  DIR     *mpDir;
  dirent  *mpEntry;

protected:
  HttpdNativeDirectory(DIR *p_dir) : mpDir(p_dir)
  {}

public:
  virtual const char *Name();
  virtual int         Next();
  
  virtual void        Close();
};

/**************************************************************************/

// File name to MIME mapping table (in sorted order)
static const HttpdPair mimeTable[] =
{
    { "aif",   "audio/aiff"                    },
    { "aiff",  "audio/aiff"                    },
    { "c",     "text/plain"                    },
    { "cpp",   "text/plain"                    },
    { "css",   "text/css"                      },
    { "gif",   "image/gif"                     },
    { "h",     "text/plain"                    },
    { "htm",   "text/html"                     },
    { "html",  "text/html"                     },
    { "jpeg",  "image/jpeg"                    },
    { "jpg",   "image/jpeg"                    },
    { "js",    "application/x-javascript"      },
    { "mid",   "audio/x-midi"                  },
    { "mov",   "video/quicktime"               },
    { "mp3",   "audio/mpeg"                    },
    { "plain", "text/plain"                    },
    { "png",   "image/png"                     },
    { "rmf",   "audio/x-rmf"                   },
    { "swf",   "application/x-shockwave-flash" },
    { "wav",   "audio/wav"                     }
};

/**************************************************************************/

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDirectory::Name
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
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
const char *HttpdNativeDirectory::Name()
{
  assert(mpEntry != NULL);
  assert(mpDir != NULL);

  return (mpEntry->d_name);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDirectory::Next
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
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdNativeDirectory::Next()
{
  assert(mpDir != NULL);

  errno   = 0;
  mpEntry = readdir(mpDir);
  if (mpEntry == NULL)
  {
    if (errno == 0)
      return (HttpdOpSys::ERR_NOTFOUND);
    else
      return (HttpdOpSys::NativeError(errno));
  }

  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDirectory::Close
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
// CREATED:        Jan 19, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdNativeDirectory::Close()
{
  assert(mpDir != NULL);

  closedir(mpDir);
  delete this;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFile::Read
//
// ACCESS:         public
//
// DESCRIPTION:    Read bytes from the file.
//
// RETURNS:        0 on success, HttpdOpSys::ERR_* on failure.
//                 On success, sz is updated with the actual number of
//                 bytes read.
//
// REMARKS:        Upon end-of-file, success is returned with a byte
//                 count of 0.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdNativeFile::Read(void *p_buffer, size_t &sz)
{
  size_t rsz = fread(p_buffer, 1, sz, mpFile);

  if ((sz != rsz) && !feof(mpFile))
    return (HttpdOpSys::NativeError(errno));
  else
  {   
    sz = rsz;
    return (0);
  }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFile::Write
//
// ACCESS:         public
//
// DESCRIPTION:    Write bytes to the file.
//
// RETURNS:        0 on success, HttpdOpSys::ERR_* on failure.
//
// REMARKS:        If the write operation is successful, then all of
//                 the bytes are written when this routine returns.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdNativeFile::Write(void *p_buffer, size_t sz)
{
  size_t wsz = fwrite(p_buffer, 1, sz, mpFile);
  
  if (sz != wsz)
    return (HttpdOpSys::NativeError(errno));
  else
    return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFile::Seek
//
// ACCESS:         public
//
// DESCRIPTION:    Change the current file position of an opened
//                 file.
//
// RETURNS:        0 on success, HttpdOpSys::ERR_* on failure.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  Jan 19. 2002  BY:  MYG (Use portable codepoints)
//
//--------------------------------------------------------------------------
int HttpdNativeFile::Seek(long offset, int whence)
{
  switch (whence)
  {
    case FILE_SEEK_START: whence = SEEK_SET; break;
    case FILE_SEEK_CUR:   whence = SEEK_CUR; break;
    case FILE_SEEK_END:   whence = SEEK_END; break;

    default:
      return (HttpdOpSys::ERR_BADPARAM);
  }

  if (fseek(mpFile, offset, whence) == -1)  
    return (HttpdOpSys::NativeError(errno));
  else
    return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFile::Close
//
// ACCESS:         public
//
// DESCRIPTION:    Destroy the HttpdFile object.
//
// REMARKS:        This method begins the destruction process. After
//                 this method is called, clients may no longer use
//                 the file object.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 19, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdNativeFile::Close()
{
  fclose(mpFile);
  delete this;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFileSystem::ComputeMime
//
// ACCESS:         public
//
// DESCRIPTION:    Get the MIME type of the file. 
//
// RETURNS:        A MIME type encoding on success or NULL on 
//                 failure.
//
//                 The returned pointer (if not NULL) is a pointer to
//                 static storage and does not need to be released.
//
// REMARKS:        How a filesystem knows the type of files is
//                 rather system dependant. In the case of POSIX,
//                 we can simply look at the extension.
//                 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
const char *HttpdNativeFileSystem::ComputeMime(const char *p_path)
{
  assert(p_path != NULL);

  char *p_saved_path = HttpdUtilities::StrVCat(p_path, (char *)0);

  if (p_saved_path != NULL)
  {
    char *p_extension = HttpdUtilities::GetLcExtension(p_saved_path);
    if (p_extension != NULL)
    {
      const char *p_mime_type = 
            HttpdUtilities::Lookup(mimeTable, 
                                   HTTPD_NUMELEM(mimeTable), 
                                   p_extension);

      HttpdOpSys::Free(p_saved_path);

      if (p_mime_type != NULL)
        return (p_mime_type);
    }
  }
  else
    return (NULL);

  // MIME type not found. Guess...
  return ("text/plain");
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFileSystem::FileInfo
//
// ACCESS:         public
//
// DESCRIPTION:    Retrieve information about the named file.
//
// RETURNS:        0 on success or HttpdOpSys::ERR_* on failure.
//
// REMARKS:        This operation is really about abstracting the
//                 name -> id translation that goes on in most
//                 filesystems. In the case of POSIX, we really
//                 just store the name as we don't want the overhead
//                 of opening it.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  Jan 18, 2002  BY:  MYG (M1007: Use struct tm)
//                 Jan 19, 2002       MYG (New filesystem abstraction)
//
//--------------------------------------------------------------------------
int HttpdNativeFileSystem::FileInfo(const char     *p_path,
                                    HttpdFileInfo  &info)
{
  char         *p_name;
  struct stat   sb;
  int           rv;

  // Do the common stuff.
  rv = CommonFileInfo(info);
  if (rv != 0)
    return (rv);

  // Save the name.
  p_name = HttpdUtilities::StrVCat(p_path, (char *)0);
  if (p_name == NULL)
    return (HttpdOpSys::ERR_OUTOFMEM);

  // Stat the file.
  if (stat(p_path, &sb) != -1)
  {
#ifdef HTTPD_HAVE_REENTRANT_LIB
    struct tm   tm[2];
    gmtime_r(&sb.st_mtime, tm);
    gmtime_r(&sb.st_ctime, tm + 1);

    info.LastModificationTime(tm);
    info.CreationTime(tm + 1);    
#else
    struct tm   *tm;

    tm = gmtime(&sb.st_mtime);
    if (tm != NULL)
      info.LastModificationTime(tm);

    tm = gmtime(&sb.st_ctime);
    if (tm != NULL)
      info.CreationTime(tm);
#endif

    bool is_dir = S_ISDIR(sb.st_mode);

    HttpdParameter p;
    p.mpVoid = (void *)p_name;   

    info.Location(p, true);
    info.Size(sb.st_size);
    info.IsDir(is_dir);

    const char *p_mime;
    if (!is_dir)
    {
      // Compute the mime type.
      p_mime = ComputeMime(p_path);
      if (p_mime == NULL) // Temporary buffer failure? 
        return (HttpdOpSys::ERR_OUTOFMEM);
    }
    else
      p_mime = "x-server-internal/directory";

    info.MimeType(p_mime, false);

    return (0);
  }
  else
  {
    HttpdOpSys::Free(p_name);
    return (HttpdOpSys::NativeError(errno));
  }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeFileSystem::Open
//
// ACCESS:         public
//
// DESCRIPTION:    Given active file information, open a file.
//
// RETURNS:        0 on success, HttpdOpSys::ERR_* on failure.
//                 On success p_file is updated to point to the
//                 file access object.
//
// REMARKS:        The file is opened as a stream of data. It
//                 is not a requirement that the info object exist
//                 while the file is open. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  Jan 19, 2002  BY:  MYG (New filesystem abstraction)
//
//--------------------------------------------------------------------------
int HttpdNativeFileSystem::Open(const HttpdFileInfo &info,
                                int                  mode,
                                HttpdFile          *&p_file)
{
  FILE        *p_fp;
  const char  *p_name = (const char *)info.Location().mpVoid;
  const char  *p_mode;

  switch (mode)
  {
    case FILE_READ_ONLY:   p_mode = "rb";  break;
    case FILE_READ_WRITE:  p_mode = "w+b"; break;

    default:
      return (HttpdOpSys::ERR_BADPARAM);
  }

  p_fp = fopen(p_name, p_mode);
  if (p_fp == NULL)
    return (HttpdOpSys::NativeError(errno));
  else
  {
    HttpdNativeFile *p_obj = new HttpdNativeFile(p_fp);

    if (p_obj == NULL)
    {
      fclose(p_fp);
      return (HttpdOpSys::ERR_OUTOFMEM);
    }

    p_file = (HttpdFile *)p_obj;
    return (0);
  }
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
// REMARKS:        We support directory iteration for native file 
//                 systems on POSIX.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 23, 2001
// LAST MODIFIED:  Jan 19, 2002  BY:  MYG (New filesystem abstraction)
//
//--------------------------------------------------------------------------
int HttpdNativeFileSystem::Open(const HttpdFileInfo  &info,
                                HttpdDirectory      *&p_dir)
{
  DIR    *p_handle;

  p_handle = opendir((const char *)info.Location().mpVoid);

  if (p_handle == NULL)
    return (HttpdOpSys::NativeError(errno));
  else
  {   
    HttpdNativeDirectory *p_obj = new HttpdNativeDirectory(p_handle);

    if (p_obj == NULL)
    {
      closedir(p_handle);
      return (HttpdOpSys::ERR_OUTOFMEM);
    }

    p_dir = (HttpdDirectory *)p_obj;
    return (0);   
  }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::NativeFileSystem
//
// ACCESS:         public
//
// DESCRIPTION:    Obtain a pointer to the operating systems
//                 native file manager.
//
// RETURNS:        NULL if no such object exists or there is a critical
//                 error or the object address on success.
//
// REMARKS:        Only one file system abstraction per OS is currently
//                 supported.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 19, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdFileSystem *HttpdOpSys::NativeFileSystem()
{
  static HttpdNativeFileSystem *p_native = NULL;

  if (p_native == NULL)
    p_native = new HttpdNativeFileSystem;

  return (p_native);
}

