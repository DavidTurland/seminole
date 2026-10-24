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
// DESCRIPTION:     Socket abstraction for Seminole on VxWorks.
//
//                  Sockets under VxWorks are quite similar to their
//                  POSIX counterparts. 
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
//  002 |11/14/01| No socklen_t type in some (most) cases.  | MYG  | M1001
// -----+--------+------------------------------------------+------+---------
//  003 |01/14/02| Add Content-Sink                         | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  004 |01/29/02| Add timeout to Gets.                     | MYG  | A1001
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
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdSocket::Shutdown()
{
  shutdown(mSocket, SHUT_RDWR);
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
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdSocket::Listen(unsigned short port)
{
  sockaddr_in sa;

  // Set up the bind address. 
  sa.sin_family      = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port        = htons(port);

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
// LAST MODIFIED:  Nov 14, 2001  BY:  MYG (M1001: No socklen_t type)
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

  /*
   * By all accounts, SO_LINGER works on all VxWorks versions I know of,
   * so just use that. Otherwise, you may have to resort to what is
   * done in the POSIX port.
   */
  struct linger linger_opt;

  linger_opt.l_onoff  = 1;
  linger_opt.l_linger = HTTPD_HCLOSE_TIMEOUT;

  setsockopt(slave, 
             SOL_SOCKET, SO_LINGER,
             (char *)&linger_opt, sizeof(linger_opt));

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
     ssize_t nwritten = write(mSocket, p_ptr, nleft);
     if (nwritten <= 0)
       return (ERR_IO);

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
// NAME:           HttpdSocket::Printf
//
// ACCESS:         public
//
// DESCRIPTION:    Socket formatted print.
//
// RETURNS:        True on success, false on error.
//
// REMARKS:        This routine is a dumbed-down printf() style routine
//                 designed to make sending HTTP response messages
//                 easier.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Oct 23, 2001  BY: MYG (%t -> unsigned long)
//
//--------------------------------------------------------------------------
bool HttpdSocket::Printf(const char *p_format, ...)
{
  va_list  va;
  char     ibuf[32], *p_arg;
  size_t   len;

  va_start(va, p_format);
 
  while (*p_format != '\0')
  {
    if (*p_format != '%')
    {
      if (WriteN((void *)p_format, sizeof(char)) != sizeof(char))
        return (false);
    }
    else
      switch (*++p_format)
      {
        case 'f': // Filler
        {
           size_t sz = va_arg(va, size_t);
           char   ch = va_arg(va, int);

           while (sz--)
           {
              if (WriteN(&ch, sizeof(char)) != sizeof(char))
                return (false);
           }
           break;
        }

        case 'd':
          sprintf(ibuf, "%d", va_arg(va, int));
          p_arg = ibuf;
          goto str_write;

        case 't':
          sprintf(ibuf, "%lu", va_arg(va, unsigned long));
          p_arg = ibuf;
          goto str_write;

        case 's':
          p_arg = va_arg(va, char *);
str_write:
          len = strlen(p_arg);
          if (WriteN(p_arg, len) != (ssize_t )len)
            return (false);
          break;

        case '%':
          if (WriteN("%", sizeof(char)) != sizeof(char))
            return (false);
          break;
      }

    p_format++;
  }

  va_end(va);
  return (true);
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
bool HttpdSocket::Gets(char *p_buf, size_t maxbuf, unsigned long timeout)
{
  char    byte;
  bool    first = true;
  time_t  start_time;
  int     nonblock;

  // Set non-blocking mode on the socket.
  nonblock = 1;
  if (fcntl(mSocket, FIONBIO, &nonblock) == -1)
    return (false);
  nonblock = 0;

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
        fd_set           read_set;
        struct timeval   tv;
        int              sret;
        
        FD_ZERO(&read_set);        
        FD_SET(mSocket, &read_set);
        
        tv.tv_sec  = (int )timeout;
        tv.tv_usec = 0;

        sret = select(mSocket + 1, &read_set, NULL, NULL, &tv);

        bool timedout  = (sret == 0) 
                      && (difftime(time(NULL), start_time) >= timeout);

        if ((sret == -1) 
        ||  timedout
        ||  (FD_ISSET(mSocket, &xcpt_set) && !FD_ISSET(mSocket, &read_set)))
        {
           (void )fcntl(mSocket, FIONBIO, &nonblock);
           return (false);
        }
        else
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
  (void )fcntl(mSocket, FIONBIO, &nonblock);
  return (!first);
}

