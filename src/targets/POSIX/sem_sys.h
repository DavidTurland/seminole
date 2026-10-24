//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_sys.h
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World 
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Declarations for the Seminole micro web server on
//                  POSIX.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct 27, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/27/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |10/30/01| Add JoinAll() method for graceful        | MYG  |
//      |        | shutdown.                                |      |
// -----+--------+------------------------------------------+------+---------
//  003 |01/11/02| Make LOCALHOST a macro.                  | MYG  | M1004
//      |        | Turn seek commands & errors into enums.  |      |
// -----+--------+------------------------------------------+------+---------
//  004 |01/14/02| Add Content-Sink                         | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  005 |01/18/02| Use broken out time.                     | MYG  | M1007
// -----+--------+------------------------------------------+------+---------
//  006 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  007 |01/29/02| Add timeout to Gets.                     | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  008 |05/04/02| Move SHUT_XXX macros out of this public  | MYG  | A1001
//      |        | header file.                             |      |
// -----+--------+------------------------------------------+------+---------
//  009 |05/09/02| Generate HttpdUint* automatically based  | SPC  | S1002
//      |        | upon values in limit.h                   |      |
// -----+--------+------------------------------------------+------+---------
//  010 |06/01/02| Add memory limits.                       | MYG  | M1017
// -----+--------+------------------------------------------+------+---------
//  011 |06/22/02| Add thread limits.                       | MYG  | M1018
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#ifndef SEM_SYS_HTTPD_H
#define SEM_SYS_HTTPD_H

#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/socket.h>

// Generate the appropriate definitions for various types, based upon
// values in <limit.h>  The values used are in common use for systems
// based upon 8bit bytes.  If your system does not fit into this catagory,
// then you will have to add the appropritate definitions.

#if (UCHAR_MAX == 255U)
   typedef unsigned char HttpUint8;
#else
#  error No integral type is 8 bits on this platform.
#endif

#if (UINT_MAX == 65535U)
  typedef unsigned int HttpdUint16;
#elif (USHRT_MAX == 65535U)
  typedef unsigned short HttpdUint16;
#elif (UCHAR_MAX == 65535U)
  typedef unsigned char HttpdUint16;
#else
#  error No integral type is 16 bits on this platform.
#endif

#if (UINT_MAX == 4294967295UL)
  typedef unsigned int HttpdUint32;
#elif (ULONG_MAX == 4294967295UL)
  typedef unsigned long HttpdUint32;
#elif (USHRT_MAX == 4294967295UL)
  typedef unsigned short HttpdUint32;
#elif (UCHAR_MAX == 4294967295UL)
  typedef unsigned char HttpdUint32;
#else
#  error No integral type is 32 bits on this platform.
#endif

// This typedef is a 32-bit IP address:
typedef HttpdUint32  HttpdIpAddress;
typedef HttpdUint16  HttpdIpPort;

// HTTPD_CONN_BACKLOG is specific to BSD sockets currently, and may have
// no meaning when using other networking API's.  VxWorks is said to have 
// problems when this value is not equal to 5, so use caution.  
// See listen(2) for more information.
const int    HTTPD_CONN_BACKLOG     = 5;   // # of pending connections

#define HTTPD_HAVE_REENTRANT_LIB
#define HTTPD_HAVE_NATIVE_FILE_SOURCES

#if !defined(HTTPD_USE_FORK) && !defined(HTTPD_USE_SINGLE)
# define HTTPD_USE_PTHREAD
#else
# if defined(HTTPD_MAX_THREAD_USAGE)
#  error HTTPD_MAX_THREAD_USAGE is not functional unless using threads.
# endif
#endif

struct HttpdOpSys
{
  friend class HttpdSocket;

  // Internal classes that need access:
  friend class HttpdNativeFileSystem;
  friend class HttpdNativeFile;
  friend class HttpdNativeDirectory;
  friend class HttpdNativeDataSource;

  /* Generic error constants */
  enum
  {
    ERR_NOTFOUND  = 1,  // no such file/directory
    ERR_SYSPERM   = 2,  // administrative permission denied
    ERR_NOTREADY  = 3,  // device/resource unavailable
    ERR_LIMITRCHD = 4,  // maximum size or limit reached
    ERR_IO        = 5,  // low-level I/O error
    ERR_WRONGTYPE = 6,  // inappropriate type for operation
    ERR_OUTOFMEM  = 7,  // ran out of memory
    ERR_BADPARAM  = 8,  // invalid parameter/argument
    ERR_BADFORMAT = 9,  // invalid data format
    ERR_UNKNOWN   = 99  // unknown error
  };

  /* Tasking abstractions */
  
  static bool        Fork(void (*p_func)(HttpdParameter p1, 
                                         HttpdParameter p2,
                                         HttpdParameter p3),
                          HttpdParameter p1,
                          HttpdParameter p2,
                          HttpdParameter p3);

  static void JoinAll();

  /* Memory abstractions */

  static void *Malloc(size_t sz);
  static void  Free(void *p_ptr);
  static void *Realloc(void *p_oldptr, size_t newsz);
  
  /* Filesystem Abstractions */

  static HttpdFileSystem *NativeFileSystem();
  static int OpenSystemFile(const char *p_filename,
                            HttpdDataSource *&p_source);
  static void CloseSystemFile(HttpdDataSource *p_source);

protected:
  static int  NativeError(int syserrno);  
};

#define HTTPD_LOCALHOST ((HttpdIpAddress )0x7F000001) // == "127.0.0.1"

class HttpdSocket : public HttpdWritable
{
  int    mSocket;

public:
  int         Write(size_t nbytes, const void *ptr);
  ssize_t     ReadN(void *ptr, size_t nbytes);
  bool        Gets(char        *p_buf, 
                   size_t       maxbuf, 
                   unsigned int timeout);
  
  bool        Socket();
  void        Close();
  bool        Accept(HttpdParameter &sock, HttpdIpAddress &client_addr);
  bool        Socket(HttpdParameter param);
  static void Cancel(HttpdParameter param);
  void        Shutdown();
  bool        Connect(HttpdIpAddress addr, unsigned short port);
  bool        Listen(unsigned short port);
};

#endif

