//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_sock.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Socket abstraction for Seminole.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Mar 28, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |03/28/01| File creation.                           | MYG  |  
// -----|--------|------------------------------------------|------|---------
//  002 |08/18/01| Cosmetic addition of SHUT_RDWR, use limit| AMH  |
//      |        | for listen() queue backlog created in    |      |
//      |        | `seminole.h'.  Typo fixes.               |      |
// -----+--------+------------------------------------------+------+---------
//  003 |09/18/01| Fix bug in error handling in Gets().     | MYG  |
// -----+--------+------------------------------------------+------+---------
//  004 |10/23/01| Change %t in Printf for new filesystem   | MYG  |
//      |        | interface.                               |      |
// -----+--------+------------------------------------------+------+---------
//  005 |10/30/01| Add SO_REUSEADDR option so we can reuse  | AMH  |
//      |        | a specific well-known port address when  | MYG  |
//      |        | restarting.                              | SPC  |
// -----+--------+------------------------------------------+------+---------
//  006 |01/14/02| Add Content-Sink                         | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  007 |01/29/02| Add timeout to Gets.                     | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  008 |02/26/02| Set TCP_NODELAY option.                  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  009 |05/04/02| Either use SO_LINGER or a blocking       | MYG  | A1001
//      |        | Shutdown() to support pipelined          | SPC  |
//      |        | connections.                             |      |
// -----+--------+------------------------------------------+------+---------
//  010 |05/07/02| Fix problem where CPU can spin on some   | JC   | C0001
//      |        | platforms.                               |      |
// -----+--------+------------------------------------------+------+---------
//  011 |06/22/02| Add option to use the poll() syscall.    | MYG  | M1020
// -----+--------+------------------------------------------+------+---------
//  012 |06/29/02| Fix compile problem without poll().      | SPC  | S1005
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#ifdef HTTPD_USE_POLL
# include <poll.h>
#endif

// XPG4-compliance for shutdown(2), otherwise we fake it.
#ifndef SHUT_RDWR
# define SHUT_RDWR 2
#endif
#ifndef SHUT_WR
# define SHUT_WR   1 
#endif
#ifndef SHUT_RD
# define SHUT_RD   0
#endif

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Close
//
// ACCESS:         public
//
// DESCRIPTION:    Free all resources associated with a socket.
//
// REMARKS:        This does a "dirty" shutdown of the socket. See the
//                 Shutdown() method for clean shutdown.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdSocket::Close()
{
  close(mSocket);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Socket
//
// ACCESS:         public
//
// DESCRIPTION:    Create an idle socket.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        Sockets created with this method can either be used
//                 for outbound connections or to establish a listening
//                 socket.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdSocket::Socket()
{
  mSocket = socket(AF_INET, SOCK_STREAM, 0);
  return (mSocket >= 0); 
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Connect
//
// ACCESS:         public
//
// DESCRIPTION:    Connect an outbound socket to a well-known port.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        Connections are attempted until a timeout, in
//                 which case an error is returned.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdSocket::Connect(HttpdIpAddress addr, unsigned short port)
{
  sockaddr_in   sa;
  
  sa.sin_family      = AF_INET;
  sa.sin_addr.s_addr = htonl(addr); // Localhost.
  sa.sin_port        = htons(port);

  return (connect(mSocket, (sockaddr *)&sa, sizeof(sa)) >= 0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Socket
//
// ACCESS:         public
//
// DESCRIPTION:    Prepare an incoming connection for TCP I/O.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        Connections on a listening socket are processed
//                 with the Accept() method. Each potential incoming
//                 connection is encoded into an HttpdParameter
//                 bundle for further processing. This method will
//                 accept the incoming connection and prepare a socket
//                 associated with the connection.
//
//                 The socket must be closed if this routine succeeds,
//                 otherwise the request must be cancelled.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdSocket::Socket(HttpdParameter param)
{
  mSocket = param.mLong;
  return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Cancel
//
// ACCESS:         public
//
// DESCRIPTION:    Cancel an incoming connection.
//
// REMARKS:        If an Accept()ed connection is to either be
//                 abandoned or a socket creation failed, the request
//                 should be cancelled with this static method.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdSocket::Cancel(HttpdParameter param)
{
  close(param.mLong);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Shutdown
//
// ACCESS:         public
//
// DESCRIPTION:    Graceful termination of an established connection.
//
// REMARKS:        An established socket should be closed with this
//                 method.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  May  4, 2002  BY:  SPC (A1001: Pipelining support)
//                 Jun 22, 2002       MYG (M1020: Optionally use poll)
//
//--------------------------------------------------------------------------
void HttpdSocket::Shutdown()
{
#if !defined(HTTPD_AVOID_LINGER)
  shutdown(mSocket, SHUT_RDWR);
#else
  time_t            then;
  int               rc;
  char              dummybuf[256];

  shutdown(mSocket, SHUT_WR);
  then = time(NULL);
  
  do
  {
#if defined(HTTPD_USE_POLL)
    struct pollfd    fds;

    fds.fd      = mSocket;
    fds.events  = POLLIN;
    fds.revents = 0;

    rc = poll(&fds, 1, HTTPD_HCLOSE_TIMEOUT * 1000);
#else
    struct timeval   tv;
    fd_set           fdlist;

    FD_ZERO(&fdlist);
    FD_SET(mSocket, &fdlist);
   
    tv.tv_sec  = HTTPD_HCLOSE_TIMEOUT;
    tv.tv_usec = 0;
    rc = select(mSocket + 1, &fdlist, NULL, NULL, &tv);
#endif
  } while ((rc > 0)
       &&  (read(mSocket, dummybuf, sizeof(dummybuf)) > 0)
       &&  (((int )difftime(time(NULL), then) - 1) < HTTPD_HCLOSE_TIMEOUT));
#endif

  close(mSocket);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Listen
//
// ACCESS:         public
//
// DESCRIPTION:    Put an idle socket into a listening state.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        The socket is turned into an accepting socket 
//                 for connections from remote clients on the 
//                 specified well-known port.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  Oct 30, 2001  BY: all (add SO_REUSEADDR)
//
//--------------------------------------------------------------------------
bool HttpdSocket::Listen(unsigned short port)
{
  sockaddr_in sa;

  // Set up the bind address. 
  sa.sin_family      = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port        = htons(port);

#ifdef SO_REUSEADDR
  // Set the socket option to reuse the address.
  int one = 1;
  if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
    return (false);
#endif

  // Bind the socket.
  if (bind(mSocket, (sockaddr *)&sa, sizeof(sa)) < 0) 
    return (false);

  // Go into a listening state.
  if (listen(mSocket, HTTPD_CONN_BACKLOG) < 0)
    return (false);

  return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Accept
//
// ACCESS:         public
//
// DESCRIPTION:    Process incoming connections from a listening
//                 socket.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        Incoming requests are abstracted within an
//                 HttpdParameter object. Once a connection is
//                 returned from this method, the HttpdParameter
//                 must either be a converted to a socket for 
//                 independant operation or cancelled with the
//                 Cancel() method.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 28, 2001
// LAST MODIFIED:  Feb 26, 2002  BY:  MYG (Set NODELAY option)
//                 May  4, 2002       MYG (A1001: Pipelining support)
//
//--------------------------------------------------------------------------
bool HttpdSocket::Accept(HttpdParameter &con, HttpdIpAddress &client_addr)
{
  sockaddr_in    addr;
  socklen_t      addr_len = sizeof(addr);

  // Block for incoming connection.
  int slave = accept(mSocket,
                     (sockaddr *)&addr, 
                     &addr_len);

  if (slave < 0)
    return (false);

  /* Disable the Nagel algorithm. */
#ifdef TCP_NODELAY
  int ndelay_opt = 1;
  setsockopt(slave, 
             IPPROTO_TCP, TCP_NODELAY, 
             (char *)&ndelay_opt, (socklen_t )sizeof(ndelay_opt));
#endif

#if defined(SO_LINGER) && !defined(HTTPD_AVOID_LINGER)
  /*
   * According to the Apache group, some TCP/IP stacks do not have 
   * a functioning SO_LINGER option. If your platform does not have
   * a functioning linger, then define HTTPD_AVOID_LINGER when
   * compiling Seminole and a workaround will be used instead.
   *
   * The workaround is not ideal in that it can waste thread time
   * for things that most protocol stacks can do on their own. 
   *
   * So, try and use SO_LINGER if you can.
   *
   */
  struct linger linger_opt;

  linger_opt.l_onoff  = 1;
  linger_opt.l_linger = HTTPD_HCLOSE_TIMEOUT;

  setsockopt(slave, 
             SOL_SOCKET, SO_LINGER,
             (char *)&linger_opt, (socklen_t )sizeof(linger_opt));
#endif

  client_addr = addr.sin_addr.s_addr; 
  con.mLong   = (long )slave;

  return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Write
//
// ACCESS:         public
//
// DESCRIPTION:    Socket data push routine.
//
// RETURNS:        OS Error code
//
// REMARKS:        The BSD sockets interface changes the semantics of the
//                 write() operation so that successive attempts to retry
//                 are usually necessary. This code encapsulates the 
//                 socket writing interface. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Jan 14, 2002  BY:  MYG (A1001: Content Sink)
//
//--------------------------------------------------------------------------
int HttpdSocket::Write(size_t nbytes, const void *ptr)
{
   size_t    nleft;
   char     *p_ptr = (char *)ptr;

   nleft = nbytes;
   while (nleft > 0) 
   {
     ssize_t nwritten = write(mSocket, p_ptr, nleft);
     if (nwritten <= 0)
       return (HttpdOpSys::NativeError(errno));

     nleft -= nwritten;
     p_ptr += nwritten;
   }
   
   return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::ReadN
//
// ACCESS:         public
//
// DESCRIPTION:    Socket data pull routine.
//
// RETURNS:        The number of bytes read on success or < 0 on 
//                 error. 
//
// REMARKS:        The BSD sockets interface changes the semantics of the
//                 read() operation so that successive attempts to retry
//                 are usually necessary. This code encapsulates the 
//                 socket reading interface. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
ssize_t HttpdSocket::ReadN(void *ptr, size_t nbytes)
{
  size_t  nleft;
  char   *p_ptr = (char *)ptr;

  nleft = nbytes;
  while (nleft > 0) 
  {
    ssize_t nread = read(mSocket, p_ptr, nleft);
    if (nread < 0)
      return (nread); 
    else if (nread == 0)
      break;

    nleft -= nread;
    p_ptr += nread;
  }
  
  return (nbytes - nleft);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdSocket::Gets
//
// ACCESS:         public
//
// DESCRIPTION:    Socket string input. 
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        This routine is a line-fetch off of a socket. 
//                 A newline character terminates the sequence. 
//                 It is important to remember that a carriage 
//                 return may be present in the consumed string as
//                 well as the newline (in keeping with fgets semantics).
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Jan 29, 2002  BY: MYG (A1001: Add timeout)
//                 May  7, 2002      JC  (C0001: Prevent CPU spinner)
//                 Jun 22, 2002      MYG (M1020: Optionally use poll)
//                 Jun 29, 2002      SPC (S1005: Fix syntax error)
//
//--------------------------------------------------------------------------
bool HttpdSocket::Gets(char *p_buf, size_t maxbuf, unsigned int timeout)
{
  char    byte;
  bool    first = true;
  time_t  start_time;
  int     original_flags;

  // Set non-blocking mode on the socket.
  original_flags = fcntl(mSocket, F_GETFL, 0);
  if (original_flags == -1)
    return (false);
  if (fcntl(mSocket, F_SETFL, original_flags | O_NONBLOCK) == -1)
    return (false);

  // Note the start time of the read operation.
  start_time = time(NULL);

  // Read in the string.
  while (maxbuf > 1)
  {
    ssize_t sz = read(mSocket, &byte, 1);
    if (sz == -1)
    {
      if (errno == EAGAIN) // Await for a timeout.
      {
#if defined(HTTPD_USE_POLL)
        struct pollfd    fds;
        int              sret;

        fds.fd      = mSocket;
        fds.events  = POLLIN | POLLERR;
        fds.revents = 0;

        sret = poll(&fds, 1, timeout * 1000);
        bool fatal_error =  (sret == 1) 
                        &&  (fds.revents & POLLERR)
                        && !(fds.revents & POLLIN);
#else
        fd_set           read_set, xcpt_set;
        struct timeval   tv;
        int              sret;
        
        FD_ZERO(&read_set);
        FD_ZERO(&xcpt_set);
        
        FD_SET(mSocket, &read_set);
        FD_SET(mSocket, &xcpt_set);
        
        tv.tv_sec  = (int )timeout;
        tv.tv_usec = 0;

        sret = select(mSocket + 1, &read_set, NULL, &xcpt_set, &tv);
        bool fatal_error =  (sret == 1) 
                         && FD_ISSET(mSocket, &xcpt_set) 
                         && !FD_ISSET(mSocket, &read_set);
#endif
     
        bool timedout  = (sret == 0) 
                      && (difftime(time(NULL), start_time) >= timeout);

        if ((sret < 0) 
        ||  timedout
        ||  fatal_error)
        {
           (void )fcntl(mSocket, F_SETFL, original_flags);
           return (false);
        }

        continue;
      }
      else // errno != EAGAIN
        break;
    }
    else if (sz == 0)
      break; // End of input.

    first    = false;
    *p_buf++ = byte;
    maxbuf--;

    if (byte == '\n')
      break;
  }

  *p_buf = '\0';
  (void )fcntl(mSocket, F_SETFL, original_flags);
  return (!first);
}
