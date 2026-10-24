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
// DESCRIPTION:     Operating System Abstraction (reference Win32 
//                  implementation)
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Nov 15, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |11/15/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |01/18/02| Use broken out time.                     | MYG  | M1007
// -----+--------+------------------------------------------+------+---------
//  003 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  004 |06/01/02| Add memory limits.                       | MYG  | M1017
// -----+--------+------------------------------------------+------+---------
//  005 |06/02/02| Thread limits.                           | MYG  | M1018
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

/****************************************************************************/
/*                            MEMORY ABSTRACTION                            */
/****************************************************************************/

#ifdef HTTPD_MAX_HEAP_USAGE

static HANDLE    httpd_heap = 0;

//---------------------------------------------------------------------------
//
// NAME:           SetupHeap
//
// DESCRIPTION:    Initialize the subheap for Seminole.
//
// RETURNS:        A heap object on success, 0 on failure.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun  1, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static HANDLE SetupHeap()
{
  if (httpd_heap != 0)
    return (httpd_heap);

  httpd_heap = HeapCreate(0, 16384, HTTPD_MAX_HEAP_USAGE);
  return (httpd_heap);
}

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
// CREATED:        Nov 15, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void *HttpdOpSys::Malloc(size_t sz)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  HANDLE heap = SetupHeap();
  if (heap == 0)
    return (NULL);
  else
    return ((void *)HeapAlloc(heap, 0, (SIZE_T )sz));
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
// CREATED:        Nov 15, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void HttpdOpSys::Free(void *p_ptr)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  if (p_ptr != NULL)
    HeapFree(SetupHeap(), 0, (LPVOID )p_ptr);
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
// CREATED:        Nov 15, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void *HttpdOpSys::Realloc(void *p_oldptr, size_t newsz)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  HANDLE heap = SetupHeap();

  if (heap == NULL)
    return (NULL);
  
  LPVOID p_result = HeapReAlloc(heap, 0, (LPVOID )p_oldptr, (SIZE_T )newsz);
  if (p_result == NULL)
    HeapFree(heap, 0, (LPVOID )p_oldptr);

  return ((void *)p_result);
#else
  void *p_result = realloc(p_oldptr, newsz);
  
  if (p_result == NULL)
    HttpdOpSys::Free(p_oldptr);

  return (p_result);
#endif
}

/****************************************************************************/
/*                           TASKING ABSTRACTION                            */
/****************************************************************************/

struct ThreadParameterBundle
{
  HttpdParameter   p1;
  HttpdParameter   p2;
  HttpdParameter   p3;
  void           (*func)(HttpdParameter p1, 
                         HttpdParameter p2,
                         HttpdParameter p3);
};

static HANDLE        hCreationEvent = 0;
static HANDLE        hCountMutex;

static unsigned long runCount       = 0;

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::JoinAll
//
// ACCESS:         public
//
// DESCRIPTION:    Join all threads spawned with Fork.
//
// REMARKS:        This method waits for all threads spawned with 
//                 Fork to die off. Threads can not be created
//                 during this call. 
//
//                 In addition, it only makes sense to call this if
//                 no more threads will be spawned (at least, relative
//                 to HttpdOpSys::Fork).
//
// AUTHOR:         M. Grosberg   
// CREATED:        Nov 15, 2001
// LAST MODIFIED:  Jun  2, 2002  BY:  MYG (M1018: Thread limits)
//
//--------------------------------------------------------------------------
void HttpdOpSys::JoinAll()
{
   bool gone  = false;
   bool first = true;

   ResetEvent(hCreationEvent);

   while (!gone)
   {
     if (first)
       first = false;
     else
       Sleep(1000UL);

     WaitForSingleObject(hCountMutex, INFINITE);
     gone = (runCount == 0);
     ReleaseMutex(hCountMutex);
   }

   SetEvent(hCreationEvent);
}

//---------------------------------------------------------------------------
//
// NAME:           ThreadGate
//
// DESCRIPTION:    Callback procedure for the Kernel.
//
// REMARKS:        This routine obtains the parameters from the
//                 common exchange area and puts them on the stack
//                 for the actual thread routine.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static void __cdecl ThreadGate(void *p_arg)
{
  ThreadParameterBundle local_params;

  // Copy the args (save some heap space)
  local_params = *((ThreadParameterBundle *)p_arg);
  free(p_arg);

  // Give this thread a TCP/IP context (why do I have to do this?)
  WORD     wsvers = MAKEWORD(2, 0);
  WSADATA  wsadata;

  if (WSAStartup(wsvers, &wsadata) == 0)
  {
    // Call thread procedure.
    (local_params.func)(local_params.p1, local_params.p2, local_params.p3);

    // Cleanup the context.
    WSACleanup();
  }

  // Decrement us from the run count.
  WaitForSingleObject(hCountMutex, INFINITE);
  runCount--;
  ReleaseMutex(hCountMutex);
}


//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Fork
//
// ACCESS:         public
//
// DESCRIPTION:    Spawn a new thread. 
//
// RETURNS:        True if the thread was spawned, false if there was 
//                 an error.
//
// REMARKS:        Three parameters are passed to the newly created
//                 thread in the form of HttpdParameter. 
//
//                 It is expected that all system calls provided by
//                 HttpdOpSys are reentrant and thread-safe.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Nov 15, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdOpSys::Fork(void (*p_func)(HttpdParameter p1, 
                                     HttpdParameter p2,
                                     HttpdParameter p3),
                      HttpdParameter p1,
                      HttpdParameter p2,
                      HttpdParameter p3)
{
   // If this is our first time, initialize some handles.
   if (hCreationEvent == 0)
   {
      hCreationEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
      if (hCreationEvent == 0)
        return (false);
      hCountMutex = CreateMutex(NULL, FALSE, NULL);
      if (hCountMutex == 0)
      {
        CloseHandle(hCreationEvent);
        hCreationEvent = 0;
        return (false);
      }
   }

   // Wait for a join to be complete.
   WaitForSingleObject(hCreationEvent, INFINITE);

   // Get a parameter buffer.
   ThreadParameterBundle *p_bundle = 
       (ThreadParameterBundle *)malloc(sizeof(ThreadParameterBundle));
   if (p_bundle == NULL)
     return (false);

   // Push the parameters in.
   p_bundle->p1   = p1;
   p_bundle->p2   = p2;
   p_bundle->p3   = p3;
   p_bundle->func = p_func;

#if defined(HTTPD_MAX_THREAD_USAGE) && defined(HTTPD_MAX_WAIT_FREE_TASK)
#if !defined(HTTPD_MAX_WAIT_FREE_TASK_INCR)
#  error HTTPD_MAX_WAIT_FREE_TASK_INCR must be defined.
#endif
   // Quota check.
   bool limit_reached;
   for(unsigned long wt = 0; 
       wt <  HTTPD_MAX_WAIT_FREE_TASK; 
       wt += HTTPD_MAX_WAIT_FREE_TASK_INCR)
   {
     WaitForSingleObject(hCountMutex, INFINITE);
     limit_reached = (runCount > HTTPD_MAX_THREAD_USAGE);
     if (!limit_reached)
       runCount++;
     ReleaseMutex(hCountMutex);

     if (limit_reached)
       Sleep(HTTPD_MAX_WAIT_FREE_TASK);
     else
       break;
   }

   // If we couldn't get past the quota after blocking for a little 
   // while, give up. The request will get denied. 
   if (limit_reached)
   {
     free(p_bundle);
     return (false);
   }
#elif defined(HTTPD_MAX_THREAD_USAGE)
   WaitForSingleObject(hCountMutex, INFINITE);
   if (runCount > HTTPD_MAX_THREAD_USAGE)
   {
     ReleaseMutex(hCountMutex);
     free(p_bundle);
     return (false);
   }
   runCount++;
   ReleaseMutex(hCountMutex);
#else
   // Increment the count.
   WaitForSingleObject(hCountMutex, INFINITE);
   runCount++;
   ReleaseMutex(hCountMutex);
#endif

   int result = _beginthread(ThreadGate, 0, p_bundle);
   if (result == -1)
   {
     free(p_bundle);
     WaitForSingleObject(hCountMutex, INFINITE);
     runCount--;
     ReleaseMutex(hCountMutex);

     return (false);
   }
   else
     return (true);
}

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

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::SystemError
//
// ACCESS:         protected
//
// DESCRIPTION:    Translate GetLastError() values into generic 
//                 Seminole ones.
//
// AUTHOR:         M. Grosberg
// CREATED:        Jan 22, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdOpSys::SystemError(unsigned long syserr)
{
  switch (syserr)
  {
    case ERROR_NO_MORE_FILES:
    case ERROR_FILE_NOT_FOUND:     return ERR_NOTFOUND;

    case ERROR_WRITE_PROTECT:
    case ERROR_INVALID_ACCESS:
    case ERROR_ACCESS_DENIED:      return ERR_SYSPERM;

    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:  return ERR_OUTOFMEM;
    
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_NO_PROC_SLOTS:
    case ERROR_DISK_FULL:
    case ERROR_MAX_THRDS_REACHED:  return ERR_LIMITRCHD;

    case ERROR_NOT_SUPPORTED:
    case ERROR_CURRENT_DIRECTORY:
    case ERROR_NOT_SAME_DEVICE:
    case ERROR_INVALID_PARAMETER:
    case ERROR_CALL_NOT_IMPLEMENTED:
    case ERROR_BAD_ARGUMENTS:
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BAD_LENGTH:         return ERR_BADPARAM;

    // It seems that most Win32 errors are some form of I/O error, 
    // so, if it's not one we recognize, treat it as an I/O error.
    default:                       return ERR_IO;
  }
}

/****************************************************************************/
/*                             FILE ABSTRACTION                             */
/****************************************************************************/

class HttpdNativeDataSource : public HttpdDataSource
{
  friend class HttpdOpSys;

  void           *mpMapping;
  unsigned long   mSize;
  HANDLE          mMappingObject;

protected:
   int MapFile(const char *p_pathname);

public:
   virtual int  ReadAt(void           *p_data,
                       size_t          sz,
                       unsigned long   offset);
   virtual const void *AddressOf(unsigned long offset, size_t sz);

   virtual ~HttpdNativeDataSource();
   HttpdNativeDataSource()
   : mpMapping(NULL), mSize(0), mMappingObject(0)
   {}
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdNativeDataSource::~HttpdNativeDataSource
//
// ACCESS:         public
//
// DESCRIPTION:    Destructor for HttpdNativeDataSource.
//
// RETURNS:        OS Error code 
//
// REMARKS:        Because the MapFile() call may leave some resources
//                 allocated and others not, we test each resource in
//                 turn.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdNativeDataSource::~HttpdNativeDataSource()
{
  if (mpMapping != NULL)
    UnmapViewOfFile(mpMapping);
  if (mMappingObject != 0)
    CloseHandle(mMappingObject);
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
// REMARKS:        The file handle is closed as soon as possible to 
//                 conserve on the system handle table.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 25, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdNativeDataSource::MapFile(const char *p_pathname)
{
  HANDLE   h_file;
  
  // Open file.
  h_file = CreateFile(p_pathname,
                      GENERIC_READ,
                      FILE_SHARE_READ,
                      NULL,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);

  if (h_file == 0)
    return (HttpdOpSys::SystemError(GetLastError()));

  // Get size.
  DWORD sz = GetFileSize(h_file, NULL);
  if (sz == INVALID_FILE_SIZE)
  {
    CloseHandle(h_file);
    return (HttpdOpSys::SystemError(GetLastError()));
  }
  mSize = (unsigned long )sz;

  // Create mapping.
  mMappingObject = CreateFileMapping(h_file,
                                     NULL,
                                     PAGE_READONLY,
                                     0, 0,
                                     NULL);
  DWORD last_err = GetLastError();
  CloseHandle(h_file);
  if (mMappingObject == 0)
    return (HttpdOpSys::SystemError(last_err));  

  // Map the mapping into the MDL.
  mpMapping = MapViewOfFile(mMappingObject,
                            FILE_MAP_READ,
                            0, 0, 0);
  if (mpMapping == NULL)
    return (HttpdOpSys::SystemError(GetLastError()));

  return (0);
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
  if ((offset + sz) > mSize)
    return (HttpdOpSys::ERR_LIMITRCHD);

  memcpy(p_data, ((char *)mpMapping) + offset, sz);
  return (0);
}

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
  if ((offset + sz) > mSize)
    return (NULL);

  return ((const void *)(((char *)mpMapping) + offset));
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
// REMARKS:        Open a system file using the Win32 memory manager.
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
