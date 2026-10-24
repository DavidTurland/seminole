//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_opsys.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Operating System Abstraction (reference POSIX 
//                  implementation)
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct  6, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/06/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |10/23/01| Abstract the filesystem.                 | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  003 |01/18/02| Use broken out time.                     | MYG  | M1007
// -----+--------+------------------------------------------+------+---------
//  004 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |06/01/02| Add memory limits.                       | MYG  | M1017
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#ifdef HTTPD_USE_PTHREAD
# include <pthread.h>
#endif

/****************************************************************************/
/*                            MEMORY ABSTRACTION                            */
/****************************************************************************/

#ifdef HTTPD_MAX_HEAP_USAGE

typedef union
{
  size_t          sz;
  
#ifdef HTTPD_DOUBLE_ALIGNMENT
  double          align;
#else
  unsigned long   align;
#endif
} HttpdHeapHeader;

static size_t   httpd_heap_used = 0;

# if defined(HTTPD_USE_PTHREAD)
static pthread_mutex_t heap_lock = PTHREAD_MUTEX_INITIALIZER;
# endif

#endif

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Malloc
//
// ACCESS:         public
//
// DESCRIPTION:    Allocate uninitialized memory.
//
// RETURNS:        A pointer to a buffer of at-least the requested
//                 size aligned to allow the most restrictive type
//                 to exist at the returned pointer. 
//
//                 NULL is returned on error.
//
// REMARKS:        The HttpdOpSys memory allocation interface provides
//                 a way to track and control all memory usage of Seminole.
//
//                 Thus, it is possible to limit the amount of memory
//                 allocated by Seminole at this point.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void *HttpdOpSys::Malloc(size_t sz)
{
#ifdef HTTPD_MAX_HEAP_USAGE

# if defined(HTTPD_USE_PTHREAD)
  pthread_mutex_lock(&heap_lock);
# endif

  if ((httpd_heap_used + sz + sizeof(size_t)) > HTTPD_MAX_HEAP_USAGE)
  {
# if defined(HTTPD_USE_PTHREAD)
    pthread_mutex_unlock(&heap_lock);
# endif
    return (NULL);
  }

  HttpdHeapHeader *p_size = (HttpdHeapHeader *)malloc(sz + sizeof(size_t));
  if (p_size == NULL)
  {
# if defined(HTTPD_USE_PTHREAD)
    pthread_mutex_unlock(&heap_lock);
# endif

    return (NULL);
  }

  httpd_heap_used += (p_size->sz = (sz + sizeof(size_t)));

# if defined(HTTPD_USE_PTHREAD)
  pthread_mutex_unlock(&heap_lock);
# endif

  return ((void *)(p_size + 1));
#else
  return malloc(sz);
#endif
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Free
//
// ACCESS:         public
//
// DESCRIPTION:    Release allocated memory.
//
// REMARKS:        The HttpdOpSys memory allocation interface provides
//                 a way to track and control all memory usage of Seminole.
//
//                 Thus, it is possible to limit the amount of memory
//                 allocated by Seminole at this point.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void HttpdOpSys::Free(void *p_ptr)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  if (p_ptr == NULL)
    return;

# if defined(HTTPD_USE_PTHREAD)
  pthread_mutex_lock(&heap_lock);
# endif

  HttpdHeapHeader *p_size = (HttpdHeapHeader *)p_ptr;
  p_size--;

  assert(httpd_heap_used > p_size->sz);
  httpd_heap_used -= p_size->sz;

# if defined(HTTPD_USE_PTHREAD)
  pthread_mutex_unlock(&heap_lock);
# endif

  free(p_size);
#else
  free(p_ptr);
#endif
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Realloc
//
// ACCESS:         public
//
// DESCRIPTION:    Change the size of an allocated memory block.
//
// RETURNS:        A pointer to the new memory block on success
//                 or NULL on failure (the original block is no longer
//                 valid on failure). 
//
// REMARKS:        The HttpdOpSys memory allocation interface provides
//                 a way to track and control all memory usage of Seminole.
//
//                 Thus, it is possible to limit the amount of memory
//                 allocated by Seminole at this point.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void *HttpdOpSys::Realloc(void *p_oldptr, size_t newsz)
{
#ifdef HTTPD_MAX_HEAP_USAGE

  HttpdHeapHeader *p_size         = (HttpdHeapHeader *)p_oldptr;
  size_t           old_block_size = (p_size == NULL)
                                  ? 0
                                  : (--p_size)->sz;

# if defined(HTTPD_USE_PTHREAD)
  pthread_mutex_lock(&heap_lock);
# endif

  if ((httpd_heap_used + newsz + sizeof(size_t) - old_block_size) 
   >  HTTPD_MAX_HEAP_USAGE)
  {
# if defined(HTTPD_USE_PTHREAD)
    pthread_mutex_unlock(&heap_lock);
# endif
    free(p_size);
    return (NULL);
  }

  HttpdHeapHeader *p_new = (HttpdHeapHeader *)realloc(p_size, 
                                                      newsz + sizeof(size_t));
  if (p_new == NULL)
  {
# if defined(HTTPD_USE_PTHREAD)
    pthread_mutex_unlock(&heap_lock);
# endif
    free(p_size);
    return (NULL);
  }

  httpd_heap_used += (p_new->sz = (newsz + sizeof(size_t)));
  httpd_heap_used -= old_block_size;

# if defined(HTTPD_USE_PTHREAD)
  pthread_mutex_unlock(&heap_lock);
# endif

  return ((void *)(p_new + 1));
#else
  void *p_result = realloc(p_oldptr, newsz);
  
  if (p_result == NULL)
    HttpdOpSys::Free(p_oldptr);

  return (p_result);
#endif
}

/****************************************************************************/

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::NativeError
//
// ACCESS:         protected
//
// DESCRIPTION:    Translate OS error values into generic Seminole ones.
//
// AUTHOR:         A. Hoerter
// CREATED:        Oct 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdOpSys::NativeError(int syserrno)
{
  switch (syserrno)
  {
    case EIO:     return ERR_IO;
    case ENOENT:  return ERR_NOTFOUND;
    case EACCES:  return ERR_SYSPERM;
    case EMFILE:  return ERR_LIMITRCHD;
    case ENFILE:  return ERR_LIMITRCHD;
    case EPIPE:   return ERR_NOTREADY;
    case EFBIG:   return ERR_LIMITRCHD;
    case ENOSPC:  return ERR_LIMITRCHD;
    case EINVAL:  return ERR_BADPARAM;

    default:
      return ERR_UNKNOWN;    
  }  
  // Not reached.
}

/****************************************************************************/
/*                             FILE ABSTRACTION                             */
/****************************************************************************/

#ifdef HTTPD_USE_MMAP

#include <sys/mman.h>

class HttpdNativeDataSource : public HttpdDataSource
{
   friend class HttpdOpSys;

   void          *mpMapping;
   unsigned long  mMaxSize;
   
protected:
   int MapFile(const char    *p_filename);
   
public:
   virtual int  ReadAt(void           *p_data,
                       size_t          sz,
                       unsigned long   offset);
   virtual const void *AddressOf(unsigned long offset, size_t sz);
                          
   HttpdNativeDataSource()
   : mpMapping(NULL), mMaxSize(0)
   {}

   virtual ~HttpdNativeDataSource();
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::~HttpdNativeDataSource
//
// ACCESS:         public
//
// DESCRIPTION:    Delete the native data source.
//
// REMARKS:        Free up the memory mapping object.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdNativeDataSource::~HttpdNativeDataSource()
{
   if (mpMapping != NULL)
     munmap(mpMapping, mMaxSize);
}


//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::MapFile
//
// ACCESS:         protected
//
// DESCRIPTION:    Map the memory of the specified file into the process
//                 address space.
//
// RETURNS:        OS Error code 
//
// REMARKS:        This version of the HttpdNativeDataSource class can be
//                 used on UNIX systems that support memory mapping. 
//
//                 In some cases this can result in a reduction of the 
//                 amount of CPU time needed to send content.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdNativeDataSource::MapFile(const char *p_filename)
{
  void         *p_result;
  struct stat   sb;
  int           fd;
  
  fd = open(p_filename, O_RDONLY, 0);
  if (fd == -1)
    goto error;
  
  if (fstat(fd, &sb) != 0)
    goto error;
  
  p_result = mmap(NULL, 
                  mMaxSize = sb.st_size, 
                  PROT_READ, MAP_PRIVATE, 
                  fd, 0);
  close(fd);           
     
  if (p_result == (caddr_t )-1)
    goto error;    

  mpMapping = p_result;
  return (0);

error:
  return (HttpdOpSys::NativeError(errno));
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::AddressOf
//
// ACCESS:         public
//
// DESCRIPTION:    Return the address of the file mapping.
//
// RETURNS:        Pointer to memory area of the specified data stream
//                 or NULL on failure.
//
// REMARKS:        To disable zero-copy optimizations, have this method
//                 return NULL.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
const void *HttpdNativeDataSource::AddressOf(unsigned long offset, size_t sz)
{
  if ((offset + sz) > mMaxSize)
    return (NULL);

  return ((const void *)(((char *)mpMapping) + offset));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::ReadAt
//
// ACCESS:         public
//
// DESCRIPTION:    Read bytes from the data source.
//
// RETURNS:        OS Error code 
//
// REMARKS:        This is pretty much just a memcpy for mapped
//                 versions.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int  HttpdNativeDataSource::ReadAt(void           *p_data,
                                   size_t          sz,
                                   unsigned long   offset)
{
  if ((offset + sz) > mMaxSize)
    return (HttpdOpSys::ERR_LIMITRCHD);

  memcpy(p_data, ((char *)mpMapping) + offset, sz);
  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::OpenSystemFile
//
// ACCESS:         public
//
// DESCRIPTION:    Open a system file as a data stream
//
// RETURNS:        OS Error code 
//
// REMARKS:        This version uses virtual memory hardware to map
//                 the system file into the address space of Seminole
//                 process.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdOpSys::OpenSystemFile(const char       *p_filename,
                               HttpdDataSource *&p_source)
{
  HttpdNativeDataSource *p_native = new HttpdNativeDataSource;
  if (p_native == NULL)
    return (ERR_OUTOFMEM);

  int rv = p_native->MapFile(p_filename);
  if (rv != 0)
  {
    delete p_native;
    return (rv);
  }  

  p_source = (HttpdDataSource *)p_native;
  return (0);
}

#else /* Don't use the mmap() system call. */

class HttpdNativeDataSource : public HttpdDataSource
{
   FILE  *mpFile;

public:
   HttpdNativeDataSource(FILE *p_file)
   : mpFile(p_file)
   {}
   
   virtual ~HttpdNativeDataSource();


   virtual int  ReadAt(void          *p_data,
                       size_t         sz,    
                       unsigned long  offset);
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::~HttpdNativeDataSource
//
// ACCESS:         public
//
// DESCRIPTION:    Delete the native data source.
//
// REMARKS:        Free up the memory mapping object.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdNativeDataSource::~HttpdNativeDataSource()
{ 
   fclose(mpFile); 
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::ReadAt
//
// ACCESS:         public
//
// DESCRIPTION:    Read bytes from a system file source.
//
// RETURNS:        OS Error code 
//
// REMARKS:        The version for a STDIO stream.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdNativeDataSource::ReadAt(void           *p_data, 
                                  size_t          sz, 
                                  unsigned long   offset)
{
  if (fseek(mpFile, (long )offset, SEEK_SET) != 0)
    return (HttpdOpSys::NativeError(errno));

  if (fread(p_data, sizeof(char), sz, mpFile) != sz)
    return (HttpdOpSys::NativeError(errno));

  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::OpenSystemFile
//
// ACCESS:         public
//
// DESCRIPTION:    Open a system file as a data stream
//
// RETURNS:        OS Error code 
//
// REMARKS:        This version uses STDIO streams and is portable to all
//                 POSIX systems.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdOpSys::OpenSystemFile(const char       *p_filename,
                               HttpdDataSource *&p_source)
{
  FILE *p_file = fopen(p_filename, "rb");

  if (p_file == NULL)
    return (NativeError(errno));

  HttpdNativeDataSource *p_native = new HttpdNativeDataSource(p_file);
  if (p_native == NULL)
  {
    fclose(p_file);
    return (ERR_OUTOFMEM);
  }

  p_source = (HttpdDataSource *)p_native;
  return (0);
}

#endif

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::CloseSystemFile
//
// ACCESS:         public
//
// DESCRIPTION:    Close a system data stream.
//
// REMARKS:        The passed pointer p_source must have been obtained
//                 from HttpdOpSys::OpenSystemFile or the behavior
//                 is undefined.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdOpSys::CloseSystemFile(HttpdDataSource *p_source)
{
  HttpdNativeDataSource *p_native = (HttpdNativeDataSource *)p_source;

  delete p_native;
}

