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
// DESCRIPTION:     Socket abstraction for Seminole (Win32)
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
//  002 |01/14/02| Add Content-Sink                         | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  003 |01/29/02| Add timeout to Gets.                     | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  004 |02/26/02| Set TCP_NODELAY option.                  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |05/04/02| Set SO_LINGER option.                    | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  006 |05/07/02| Fix problem where CPU can spin in some   | JC   | C0001
//      |        | cases.                                   |      |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

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
  closesocket(mSocket);
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
  closesocket(param.mLong);
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
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdSocket::Shutdown()
{
  shutdown(mSocket, 2);
  closesocket(mSocket);
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
  BOOL v = TRUE;
  if (0&&setsockopt(mSocket, 
                 SOL_SOCKET, 
                 SO_REUSEADDR, 
                 (const char *)&v, 
                 sizeof(v)) == -1)
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
//                 May  4, 2002       MYG (A1001: Set linger option)
//
//--------------------------------------------------------------------------
bool HttpdSocket::Accept(HttpdParameter &con, HttpdIpAddress &client_addr)
{
  sockaddr_in    addr;
  int            addr_len = sizeof(addr);

  // Block for incoming connection.
  int slave = accept(mSocket,
                     (sockaddr *)&addr, 
                     &addr_len);

  if (slave < 0)
    return (false);

  /* Set the NODELAY flag */
  BOOL opt = TRUE;

  setsockopt(slave,
             IPPROTO_TCP, TCP_NODELAY,
             (const char *)&opt, sizeof(opt));

  /* Set the linger timeout. */
  struct linger linger_opt;

  linger_opt.l_onoff  = 1;
  linger_opt.l_linger = HTTPD_HCLOSE_TIMEOUT;

  setsockopt(slave, 
             SOL_SOCKET, SO_LINGER,
             (const char *)&linger_opt, sizeof(linger_opt));

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
// RETURNS:        OS Error code.
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
     ssize_t nwritten = send(mSocket, p_ptr, nleft, 0);
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
    ssize_t nread = recv(mSocket, p_ptr, nleft, 0);
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
//
//--------------------------------------------------------------------------
bool HttpdSocket::Gets(char *p_buf, size_t maxbuf, unsigned int timeout)
{
  char           byte;
  bool           first = true;
  time_t         start_time;
  struct timeval timeval;

  // Set non-blocking mode on the socket.
  u_long nonblock = 1;
  if (ioctlsocket(mSocket, FIONBIO, &nonblock) == SOCKET_ERROR)
    return (false);
  nonblock = 0;


  // Note the start time of the read operation and timeout structure.
  start_time      = time(NULL);
  timeval.tv_sec  = timeout;
  timeval.tv_usec = 0;

  // Read in the string.
  while (maxbuf > 1)
  {
    int sz = recv(mSocket, &byte, 1, 0);
    if (sz == SOCKET_ERROR)
    {
      if (WSAGetLastError() == WSAEWOULDBLOCK) // Await for a timeout.
      {
        fd_set           read_set, xcpt_set;
        struct timeval   tv;
        int              sret;
        
        FD_ZERO(&read_set);
        FD_ZERO(&xcpt_set);
        
        FD_SET(mSocket, &read_set);
        FD_SET(mSocket, &xcpt_set);
        
        sret = select(0, &read_set, NULL, &xcpt_set, &timeval);
        bool timedout  = (sret == 0)
                      && (difftime(time(NULL), start_time) >= timeout);

        if ((sret == -1) 
        ||  timedout
        ||  (FD_ISSET(mSocket, &xcpt_set) && !FD_ISSET(mSocket, &read_set)))
        {
           (void )ioctlsocket(mSocket, FIONBIO, &nonblock);
           return (false);
        }
        else
           continue;
      }
      else // Other error.
        break;
    }
    else if (sz == 0)
      break; // End-of-input.

    first    = false;    
    *p_buf++ = byte;
    maxbuf--;

    if (byte == '\n')
      break;
  }

  *p_buf = '\0';
  (void )ioctlsocket(mSocket, FIONBIO, &nonblock);
  return (!first);
}
