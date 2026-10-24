//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_httpd.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Seminole webserver class.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Mar 13, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |03/13/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |08/18/01| Typo fixes, use HttpdSocket::LOCALHOST,  | AMH  |
//      |        | buglet in Stop().                        |      |
// -----+--------+------------------------------------------+------+---------
//  003 |08/28/01| Removed ResponseName() and ErrorPage()   | SPC  |
//      |        | as that functionality is now handled in  |      |
//      |        | the HttpdResponse class.                 |      |
//      |        | (Actually, not so much removed as just   |      |
//      |        | commented out.  How do we handle remov-  |      |
//      |        | ing functions?)                          |      |
// -----+--------+------------------------------------------+------+---------
//  004 |09/30/01| Made request strings common.             | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |10/06/01| Abstract memory allocation.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  006 |10/30/01| Support graceful shutdown and handler    | MYG  |
//      |        | deletion.                                |      |
// -----+--------+------------------------------------------+------+---------
//  007 |11/16/01| Remove unecessary var in thread method.  | MYG  | M1003
// -----+--------+------------------------------------------+------+---------
//  008 |01/11/02| Move declaration from inside for loop.   | MYG  | M1002
//      |        | Make LOCALHOST be a macro.               |      | M1004
// -----+--------+------------------------------------------+------+---------
//  009 |01/22/02| Report OS name if appropriate.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  010 |04/14/02| Add persistent connections.              | AMH  | A1001
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           Httpd::CreateListenSocket
//
// ACCESS:         private
//
// DESCRIPTION:    Create the WKS socket to accept connections on.
//
// RETURNS:        True on success, false for operating system error.
//
// REMARKS:        This is pretty standard BSD sockets stuff. Depending
//                 on specific TCP stack implementations, several options
//                 may need to be set here. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Mar 28, 2001  BY: Abstract sockets
//
//--------------------------------------------------------------------------
bool Httpd::CreateListenSocket()
{
  // Create a socket.
  if (!mListenSock.Socket())
    goto sock_failed;
  
  // Setup the bind address. 
  if (!mListenSock.Listen(mPort))
    goto listen_failed;
  
  return (true);

listen_failed:
  mListenSock.Close(); 
sock_failed:
  return (false);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::ServerName
//
// ACCESS:         public
//
// DESCRIPTION:    Return the branded name of the web server.
//
// RETURNS:        A pointer to the server name.
//
// REMARKS:        This is can be overridden in derived classes so products
//                 can put their specific brands on the resultant MIME 
//                 headers.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Jan 22, 2002  BY:  MYG (Add optional OS name)
//
//--------------------------------------------------------------------------
const char *Httpd::ServerName()
{
  return 
  (
    "Seminole/2.14"
#ifdef HTTPD_OS_NAME
    " (" HTTPD_OS_NAME ")"
#endif
  );
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Httpd
//
// ACCESS:         public
//
// DESCRIPTION:    Initialize the web server object.
//
// REMARKS:        Of course, this just sets up some member variables,
//                 the service is controlled via Start() and Stop().
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
Httpd::Httpd(HttpdIpPort port, const char *p_srv_host)
  : mpHandlers(NULL),
    mPort(port),
    mState(Stopped)
{
  mpServerHost = HttpdUtilities::StrVCat(p_srv_host, (char *)0);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::~Httpd
//
// ACCESS:         public
//
// DESCRIPTION:    Destructor of web server object.
//
// REMARKS:        We try to be a good citizen and stop the server if
//                 it is running. However, it's probably safer to do this
//                 from the application. Of course, the product could
//                 just be turned off.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
Httpd::~Httpd()
{
  if (mState == Started)
    (void )Stop();

  HttpdOpSys::Free(mpServerHost);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Start
//
// ACCESS:         public
//
// DESCRIPTION:    Start the Httpd service.
//
// RETURNS:        True if the service started, false if not.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Nov 16, 2001  BY: MYG (M1003: Remove unecessary var)
//
//--------------------------------------------------------------------------
bool Httpd::Start()
{
  HttpdParameter p1;
  
  if (mState == Started)
    return (true);

  if (mpServerHost == NULL)
    return (false);
  
  if (!CreateListenSocket())
    goto sock_failed;
  
  p1.mpVoid = (void *)this;

  if (!HttpdOpSys::Fork(Acceptor, p1, p1, p1))
    goto task_failed;
  
  mState = Started;
  return (true);

task_failed:
  mListenSock.Close();    
sock_failed:
  return (false);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Stop
//
// ACCESS:         public
//
// DESCRIPTION:    Stop the Httpd service.
//
// REMARKS:        We stop the accepting thread in an ingenious way.
//                 We connect to the listening port after setting
//                 the state to a stop pending. This causes the 
//                 accepting thread to begin processing a new connection,
//                 but it checks the state before that and sees we are
//                 coming down. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Oct 30, 2001  BY:  MYG (Graceful shutdown)
//                 Jan 14, 2002       MYG (A1001: Content Sink)
//
//--------------------------------------------------------------------------
void Httpd::Stop()
{
  HttpdSocket  sock;

  // Decide if we need to shut down.
  if (mState != Started)
    return;

  // Change the state.
  mState = Stopping;

  // Force a connect to shut down the listen task. 
  if (sock.Socket())
  {
    if (sock.Connect(HTTPD_LOCALHOST, mPort))
     sock.Write(8, "QUIT\r\n\r\n");

    sock.Close();
  }

  HttpdOpSys::JoinAll();
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Allowed
//
// ACCESS:         protected
//
// DESCRIPTION:    Determine if the client passes basic (IP address)
//                 security checks. 
//
// RETURNS:        True if the transaction should proceed, False 
//                 otherwise.
//
// REMARKS:        This is just a first-level attempt at security. 
//                 Real security can be implemented as appropriate in 
//                 the handlers where permissions and other security
//                 logic can be added.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool Httpd::Allowed(HttpdIpAddress)
{
  // By default: All clients are allowed. 
  return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Install
//
// ACCESS:         public
//
// DESCRIPTION:    Install the handler in the handler list. 
//
// REMARKS:        The service must be in a stopped state. In addition,
//                 unless special precautions are taken, if the server
//                 was previously running installing more handlers may
//                 cause a problem. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void Httpd::Install(HttpdHandler *p_handler)
{
  assert(mState == Stopped);

  p_handler->Install(mpHandlers);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Remove
//
// ACCESS:         public
//
// DESCRIPTION:    Remove the handler for the associated prefix from the 
//                 handler list. 
//
// RETURNS:        A pointer to the deleted handler if it is found
//                 and removed from the list. If the specified handler
//                 could not be found, NULL is returned. 
//
// REMARKS:        The service must be in a stopped state before
//                 removing handlers. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdHandler *Httpd::Remove(const char *p_prefix)
{
  HttpdHandler  *p_node  = mpHandlers;
  HttpdHandler **pp_prev = &mpHandlers;

  assert(mState == Stopped);

  while (p_node != NULL)
  {
     if (strcmp(p_node->mpPrefix, p_prefix) == 0)
     {
       *pp_prev       = p_node->mpNext;
       p_node->mpNext = NULL;

       return (p_node);
     }

     pp_prev = &p_node->mpNext;
     p_node  = p_node->mpNext;
  }

  return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Remove
//
// ACCESS:         public
//
// DESCRIPTION:    Remove the specified handler from the handler list. 
//
// RETURNS:        A pointer to the deleted handler if it is found
//                 and removed from the list. If the specified handler
//                 could not be found, NULL is returned.
//
// REMARKS:        The service must be in a stopped state before
//                 removing handlers. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdHandler *Httpd::Remove(HttpdHandler *p_handler)
{
  HttpdHandler  *p_node  = mpHandlers;
  HttpdHandler **pp_prev = &mpHandlers;

  assert(mState == Stopped);

  while (p_node != NULL)
  {
     if (p_node == p_handler)
     {
       *pp_prev       = p_node->mpNext;
       p_node->mpNext = NULL;

       return (p_node);
     }

     pp_prev = &p_node->mpNext;
     p_node  = p_node->mpNext;
  }

  return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Acceptor
//
// ACCESS:         protected
//
// DESCRIPTION:    Incoming connection acceptance. 
//
// REMARKS:        This is point where all incoming requests in the 
//                 server are first handled. Some quick and dirty
//                 processing is done and then a thread is spawned
//                 to handle the actual response tasks.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void Httpd::Acceptor(HttpdParameter httpd, 
                     HttpdParameter,
                     HttpdParameter)
{
  Httpd *p_server = (Httpd *)httpd.mpVoid;

  for(;;)
  {
    HttpdParameter sock_encap; 
    HttpdParameter client_addr;

    // Block for incoming connection.
    if (!p_server->mListenSock.Accept(sock_encap, client_addr.mAddress))
    {
      p_server->mState = Stopped;
      return;
    }

    // If shutting down, go away.
    if (p_server->mState == Stopping)
    {
      p_server->mState = Stopped;
      HttpdSocket::Cancel(sock_encap);
      p_server->mListenSock.Close();
      return;
    }

    // Authenticate the connection. 
    if (!p_server->Allowed(client_addr.mAddress))
    {
      HttpdSocket::Cancel(sock_encap);
      continue;
    }

    // Now fork the server process. 
    if (!HttpdOpSys::Fork(Server, httpd, sock_encap, client_addr))
      HttpdSocket::Cancel(sock_encap);
  }  
}

//---------------------------------------------------------------------------
//
// NAME:           Httpd::Server
//
// ACCESS:         protected
//
// DESCRIPTION:    Transaction processing task. 
//
// REMARKS:        This task is spawned for each incoming request. 
//                 The basic approach to handling a request is to go
//                 through the handler list searching for a handler
//                 willing to service the request. 
//
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Jan 11, 2002  BY:  MYG (M1002: C++ Portability)
//                 Apr 14, 2002       AMH (A1001: Persistent connections)
//
//--------------------------------------------------------------------------
void Httpd::Server(HttpdParameter httpd, 
                   HttpdParameter sock_encap,
                   HttpdParameter client_addr)
{
  Httpd         *p_server = (Httpd *)httpd.mpVoid;

#ifdef HTTPD_INC_PERSISTENT_CONN
  unsigned int  req_cnt;

  for (req_cnt = 0;; req_cnt++)
  {
    // Get the request, parsing the MIME headers in as well.
    HttpdRequest   request(sock_encap, 
                           client_addr.mAddress,
                           p_server);

    // If we're at our limit, make this the last request.
    if (req_cnt == HTTPD_MAX_REQUESTS_PER_CONN)
      request.SetLastReq();

    // Do it.
    p_server->ProcessRequest(&request);

    if (request.LastReq())  // Nothing more, shut down the socket.
    {
       request.Socket().Shutdown(); 
       break; 
    } 
  }
#else /* No persistent connections. */
  // Get the request, parsing the MIME headers in as well.
  HttpdRequest   request(sock_encap, 
                         client_addr.mAddress,
                         p_server);

  // Do it.
  p_server->ProcessRequest(&request);
  request.Socket().Shutdown();
#endif  
}

//---------------------------------------------------------------------------
// 
// NAME:           Httpd::ProcessRequest
// 
// ACCESS:         protected
// 
// DESCRIPTION:    Request processing method.
// 
// REMARKS:        This method is called by Server() for each successive 
//                 request coming in from a client on one connection.
//                 Its task is to validate the request and walk the
//                 handler chain looking for an appropriate handler.
//
// AUTHOR:         A. Hoerter
// CREATED:        Apr 2, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void Httpd::ProcessRequest(HttpdRequest *p_request)
{
  HttpdHandler  *p_handler;

  // First things first, if we had a problem with the headers,
  // then bail.
  if (p_request->Method() == NULL)
  {
    p_request->SetLastReq();
    return;
  }

  // HTTP v0.9: Check for invalid request before calling handlers.
  if (!p_request->NeedHeaders())
  {
     if (strcmp(p_request->Method(), HttpdUtilities::mGetRequest) != 0)
     {
        p_request->Socket().Printf(
           "<html><head><title>Error</title></head>\n"
           "<body>Your client sent an invalid %s request without a\n"
           "protocol version (assuming HTTP v0.9).\n"
           "<p>The request can not be processed.</body></html>",
           p_request->Method());

        p_request->SetLastReq();
        return;
     }
  }

  // Now, try to find a handler willing to take the request.
  for(p_handler = mpHandlers;
      p_handler != NULL;
      p_handler = p_handler->Next())
  {
    if (p_handler->Handle(p_request))
      return;
  }

  // Nothing handled the request. We are going to give up and say
  // something silly.
  p_request->Respond(HTTPD_RESP_NOT_FOUND);
}

