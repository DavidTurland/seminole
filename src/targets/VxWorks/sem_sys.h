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
//                  VxWorks.
//
// NOTE:            THIS FILE IS NOT COMPLETE!
//
//                  Details like filesystem and memory management are
//                  usually not sufficiently common between projects
//                  that they can be standard. Please modify this file
//                  to match the environment of your project.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct 30, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/30/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |01/11/02| Make LOCALHOST a global macro.           | MYG  | M1004
// -----+--------+------------------------------------------+------+---------
//  003 |01/14/02| Add Content-Sink                         | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  004 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |01/29/02| Add timeout to Gets.                     | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  006 |05/09/02| Generate HttpdUint* automatically based  | SPC  | S1002
//      |        | upon values in limit.h                   |      |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#ifndef SEM_SYS_HTTPD_H
#define SEM_SYS_HTTPD_H

#include <limits.h>
#include <sockLib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

// XPG4-compliance for shutdown(2), otherwise we fake it.
#ifndef SHUT_RDWR
# define SHUT_RDWR 2
#endif

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

struct HttpdOpSys
{
  /* Generic error constants */
  
  static const int ERR_NOTFOUND  = 1;  // no such file/directory
  static const int ERR_SYSPERM   = 2;  // administrative permission denied
  static const int ERR_NOTREADY  = 3;  // device/resource unavailable
  static const int ERR_LIMITRCHD = 4;  // maximum size or limit reached
  static const int ERR_IO        = 5;  // low-level I/O error
  static const int ERR_WRONGTYPE = 6;  // inappropriate type for operation
  static const int ERR_OUTOFMEM  = 7;  // ran out of memory
  static const int ERR_BADPARAM  = 8;  // invalid parameter/argument
  static const int ERR_BADFORMAT = 9;  // invalid data format
  static const int ERR_UNKNOWN   = 99; // unknown error

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
  
  /* File System */
  static HttpdFileSystem *NativeFileSystem();
};

#define HTTPD_LOCALHOST ((HttpdIpAddress )0x7F000001) // == "127.0.0.1"

class HttpdSocket : public HttpdWritable
{
  int    mSocket;

public:
  int         Write(size_t nbytes, const void *ptr);
  ssize_t     ReadN(void *ptr, size_t nbytes);
  bool        Gets(char *p_buf, size_t maxbuf, unsigned int timeout);
  
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

