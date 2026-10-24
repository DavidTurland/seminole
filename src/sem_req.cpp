//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_req.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     HttpdRequest class (incoming HTTP requests)
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
//  002 |08/27/01| Several modifications to handle HTTP/0.9 | SPC  |
//      |        | requests correctly.                      |      |
// -----+--------+------------------------------------------+------+---------
//  003 |09/18/01| Add continued MIME-header line support.  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  004 |09/23/01| Protocol response negotiation.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |09/30/01| Send body only when appropriate.         | MYG  |
// -----+--------+------------------------------------------+------+---------
//  006 |10/06/01| Abstract memory allocation.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  007 |10/18/01| Remove extra StrChop() in headers.       | MYG  |
// -----+--------+------------------------------------------+------+---------
//  008 |10/19/01| Use Host: header for redirects.          | MYG  |
// -----+--------+------------------------------------------+------+---------
//  009 |11/16/01| Fix bad cast in version parser.          | MYG  | M1005
// -----+--------+------------------------------------------+------+---------
//  010 |01/26/02| Watch for port in Host header.           | MYG  | M1010
// -----+--------+------------------------------------------+------+---------
//  011 |01/29/02| Improve whitespace stripping.            | MYG  | 
// -----+--------+------------------------------------------+------+---------
//  012 |02/05/02| Handle a complete URI in the request     | MYG  | M1011
// -----+--------+------------------------------------------+------+---------
//  013 |02/11/02| Add a method to determine the target     | MYG  |
//      |        | host name in the request object.         |      |
// -----+--------+------------------------------------------+------+---------
//  014 |03/29/02| Fix the sort order of the MIME header    | MYG  | M1015
//      |        | table to be case-insensitive.            |      |
// -----+--------+------------------------------------------+------+---------
//  015 |04/14/02| Add persistent connections.              | AMH  | A1001
// -----+--------+------------------------------------------+------+---------
//  016 |05/07/02| Add const to pacify some compilers.      | JC   | C0001
// -----+--------+------------------------------------------+------+---------
//  017 |05/10/02| Fix possible double-free in request      | SPC  | S1001
//      |        | parser on partial input.                 |      |
// -----+--------+------------------------------------------+------+---------
//  018 |07/11/02| Initialize mpMethod.                     | MYG  | M1030
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           CompareMime
//
// DESCRIPTION:    A callback used for sorting MIME tables. 
//
// RETURNS:        See qsort.
//
// REMARKS:        We sort the MIME header received from the client so
//                 that we can easily find values in it. I think this ends
//                 up being a little less code space than a linear search
//                 routine would take anyhow (well, qsort gets linked in). 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Mar 29, 2002  BY:  MYG (M1015: Make case-insensitive)
//
//--------------------------------------------------------------------------
static int CompareMime(const void *mime_a, const void *mime_b)
{
  const HttpdPair  *p_a = (const HttpdPair *)mime_a;
  const HttpdPair  *p_b = (const HttpdPair *)mime_b;

  return (HttpdUtilities::StrCmpi(p_a->mpKey, p_b->mpKey));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::HttpdRequest
//
// ACCESS:         public
//
// DESCRIPTION:    Constructor for an HTTP request.
//
// REMARKS:        When a connection is accepted, this constructor
//                 parses out the request line and the MIME headers
//                 and stores them away in the request for later use. 
//
//                 Error handling in constructors is always interesting;
//                 if we find that we had an error during the request
//                 handling we set the method to NULL. The request
//                 processor will check for this and terminate things
//                 gracefully.                 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Aug 27, 2001  BY:  SPC - Handle HTTP/0.9 requests
//                 Sep 18, 2001  BY:  MYG - Multi-line requests
//                 Sep 23, 2001  BY:  MYG - Internalize protocol vers.
//                 Jan 29, 2002  BY:  MYG - Improve \r\n stripping.
//                 Feb  5, 2002  BY:  MYG - M1011: Transform mpPath.
//                 Feb 11, 2002  BY:  MYG - Add lazy host name routine.
//                 Jan 14, 2002  BY:  AMH - A1001: Persistent connections.
//                 May 10, 2002  BY:  SPC - S1001: Fix double-free.
//                 Jul 11, 2002  BY:  MYG - M1030: Initialize mpMethod.
//
//--------------------------------------------------------------------------
HttpdRequest::HttpdRequest(HttpdParameter   sock, 
                           HttpdIpAddress   address,
                           Httpd           *p_server)
 : mpCompleteUri(NULL),
   mpRequestedHostName(NULL),
   mpServer(p_server),
   mAddress(address)
{
  mSocket.Socket(sock);

  bool       first  = true;

  mHeaderCount = 0;
  mNeedHeaders = false;
  mpRequest    =
  mpMethod     = NULL;
  mLastReq     = true; // Unless we determine otherwise, this request
                       // is not part of a persistent connection.
  mpHeaders    = (HttpdPair *)HttpdOpSys::Malloc(sizeof(HttpdPair) 
                                               * HTTPD_MAX_MIME_ENTRIES);
  if (mpHeaders == NULL)
    goto pair_error;

  for(;;)
  {
    char   line_buf[HTTPD_MAX_INPUT_LINE];
    size_t len;
    char   ch;

    if (!mSocket.Gets(line_buf, 
                      sizeof(line_buf), 
                      first ? HTTPD_FIRST_TIMEOUT : HTTPD_MIME_TIMEOUT))
      goto parse_error;

    if (strcmp(line_buf, "\r\n") == 0)
      break;
  
    len = strlen(line_buf);
    if (len == 0)
      continue;

    ch = line_buf[len - 1];
    if ((ch == '\n') || (ch == '\r'))
      line_buf[--len] = '\0';
    if (len == 0)
      continue;

    ch = line_buf[len - 1];
    if ((ch == '\n') || (ch == '\r'))
      line_buf[--len] = '\0';

    char *p_result = HttpdUtilities::StrVCat(line_buf, (char *)0);

    if (p_result == NULL)
      goto parse_error;

    if (first)
    {
      char *p_protocol;

      // The first line received is the request line, we handle
      // that specially because for some reason the HTTP protocol
      // did not just encode this information in the MIME headers as
      // well.  --MYG
      //                 ---
      // The initial HTTP protocol (HTTP 0.9) didn't even have a header
      // section, just the initial GET method.  Headers came with a later
      // revision of the protocol spec, but like most things, once it's done,
      // it's enshrined in stone and only very slowly changes.
      //
      // Just look at the PC architecture, which was nearly frozen for
      // fifteen to twenty years!
      //
      // Anyway, if we get an HTTP/0.9 request, there are no headers, there is
      // no blank line, there is no spoon, so skip the rest of this routine
      // for the most part.  --SPC

      first      = false;
      mpRequest  = p_result;
      mpMethod   = HttpdUtilities::StrChop(p_result);
      mpPath     = HttpdUtilities::StrChop(p_result);
      p_protocol = HttpdUtilities::StrChop(p_result);

      // No protocol version sent for HTTP/0.9
      if ((p_protocol == NULL) || (*p_protocol == '\0')) 
      {
        mRequestVersion = HTTPv09;
        break;
      }
 
      mRequestVersion = ParseHttpVersion(p_protocol);
      mNeedHeaders    = true;
    }
    else
    {
      // We must have found a MIME header line. Tokenize it and
      // and store it away. 
      if (!isspace(*p_result)) // Header continuation? Nope.
      {
        bool mime_ok = ReadMimeHeader(p_result);
        HttpdOpSys::Free(p_result);

        if (!mime_ok)
          goto parse_error;
      }
      else // Continued header.
      {
        if (mHeaderCount == 0) // Nothing to continue from...
          continue;
 
        bool mime_ok = ReadContinuedMime(p_result);
        HttpdOpSys::Free(p_result);

        if (!mime_ok)
          goto parse_error;
      }
    }
  }

  // Now sort the headers. 
  if (mHeaderCount != 0)  // just in case a buggy qsort()
    qsort(mpHeaders, mHeaderCount, sizeof(HttpdPair), CompareMime);

  // Transform the URI (if applicable)
  if (HttpdUtilities::IsUriProtocol(mpPath))
  {
    char *p_colon = strchr(mpPath, ':');
    if ((p_colon != NULL) && (p_colon[1] == '/') && (p_colon[2] == '/'))
    {

      char *p_first = strchr(p_colon + 3, '/');
      if (p_first != NULL)
      {
        mpCompleteUri = mpPath;
        mpPath        = p_first;
      }
    }
  }

#ifdef HTTPD_INC_PERSISTENT_CONN
  // Determine whether this request is only one segment of a persistent
  // connection.  HTTP/0.9 doesn't support them at all.  HTTP/1.1
  // implementations may assume that connections are persistent by
  // default (unless a `Connection: close' header is seen).  HTTP/1.0
  // requires explicit negotiation through use of the
  // `Connection: Keep-Alive' header.  Either the client or the server
  // may determine when to drop the connection (if the server only
  // replies to a subset of the client's requests, the client is required
  // to retry if responses are desired).  
  //
  // --AMH
  const char *p_connection; 

  mLastReq = (mRequestVersion < HTTPv11);
  if ((p_connection = Header("Connection")) != NULL)
  {
    if (HttpdUtilities::StrCmpi("keep-alive", p_connection) == 0)
      mLastReq = false;
    else if (HttpdUtilities::StrCmpi("close", p_connection) == 0)
      mLastReq = true;
  }
#endif

  return;

parse_error:
  FreeMimeStrings();
  mHeaderCount = 0;
  HttpdOpSys::Free(mpRequest);
  HttpdOpSys::Free(mpHeaders);

pair_error:
  mpRequest  = NULL;
  mpMethod   = NULL;
  mpHeaders  = NULL;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::ReadMimeHeader
//
// ACCESS:         private
//
// DESCRIPTION:    Read a MIME header name/value pair into the 
//                 request.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        The contents of p_result are destroyed in the
//                 process.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Sep 18, 2001
// LAST MODIFIED:  Oct 18, 2001  BY:  MYG (Remove extra StrChop)
//
//--------------------------------------------------------------------------
bool HttpdRequest::ReadMimeHeader(char *p_result)
{
  char *p_key, *p_trimmer;

  if (mHeaderCount == HTTPD_MAX_MIME_ENTRIES)
    return (false); // No more room.

  p_key = HttpdUtilities::StrChop(p_result);

  p_trimmer = strrchr(p_key, ':');
  if (p_trimmer != NULL)
    *p_trimmer = '\0';

  p_key    = HttpdUtilities::StrVCat(p_key, (char *)0);
  p_result = HttpdUtilities::StrVCat(p_result, (char *)0);

  mpHeaders[mHeaderCount].mpKey   = p_key;
  mpHeaders[mHeaderCount].mpValue = p_result;
      
  mHeaderCount++;

  return ((p_key != NULL) && (p_result != NULL));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::ReadContinuedMime
//
// ACCESS:         private
//
// DESCRIPTION:    Read additional MIME header value data into the 
//                 request.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        There must be at least one entry in the headers
//                 table before this routine is called.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Sep 18, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdRequest::ReadContinuedMime(char *p_result)
{
  assert(mHeaderCount > 0);

  size_t        prev_index = mHeaderCount - 1;
  const char   *p_prev     = mpHeaders[prev_index].mpValue;
       
  while ((*p_result != '\0') && isspace(*p_result))
          p_result++;

  char   *p_newval = HttpdUtilities::StrVCat(p_prev, 
                                             p_result,
                                             (char *)0);
  if (p_newval == NULL)
    return (false);
  else
  {
    HttpdOpSys::Free((void *)p_prev);
    mpHeaders[prev_index].mpValue = p_newval;
  }

  return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::~HttpdRequest
//
// ACCESS:         public
//
// DESCRIPTION:    Shut down a completed request.
//
// REMARKS:        Clean up all allocated resources (buffer list, etc) 
//                 from the request. We have to handle both the 
//                 successful (method != NULL) and errored (method == NULL)
//                 cases here!
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Feb 11, 2002  BY:  MYG - Add lazy host name routine.
//
//--------------------------------------------------------------------------
HttpdRequest::~HttpdRequest()
{
  FreeMimeStrings();

  HttpdOpSys::Free(mpRequest);
  HttpdOpSys::Free(mpHeaders);
  HttpdOpSys::Free(mpRequestedHostName);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::FreeMimeStrings
//
// ACCESS:         private
//
// DESCRIPTION:    Free all allocated storage for MIME header values and
//                 keys.
//
// REMARKS:        This is a utiliity routine for handling cleanup
//                 of requests.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Sep 18, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdRequest::FreeMimeStrings()
{
  if (mpHeaders != NULL)
    for(size_t i = 0; i < mHeaderCount; i++)
    {
        HttpdOpSys::Free((void *)mpHeaders[i].mpKey);
        HttpdOpSys::Free((void *)mpHeaders[i].mpValue);
    }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::RequestedHostName
//
// ACCESS:         public
//
// DESCRIPTION:    Determine the host name associated with the request.
//
// RETURNS:        A pointer to the host name on success. NULL on
//                 failure.
//
// REMARKS:        There are many ways we can get the host information
//                 from a request. This method tries things in the order
//                 of most common to least common. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Feb 11, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
const char *HttpdRequest::RequestedHostName()
{
  if (mpRequestedHostName == NULL)
  {
    const char *p_host = Header("Host");
    if (p_host != NULL)
    {
       char        port_str[8];
       HttpdIpPort port = mpServer->Port();

       // Approach #1: See if a Host: header was sent. 
       if ((port == 80) || (strchr(p_host, ':') != NULL))
         port_str[0] = '\0';
       else
         sprintf(port_str, ":%lu", (unsigned long )port);
       
       mpRequestedHostName = HttpdUtilities::StrVCat(p_host, 
                                                     port_str, 
                                                     (char *)0);
    }
    else if ((CompleteUri() != NULL) 
    &&       (HttpdUtilities::HasPrefix(CompleteUri(), "http://")))
    {
       // Approach #2: See if a full URI was presented in the method.
       mpRequestedHostName = HttpdUtilities::StrVCat(CompleteUri() + 7,
                                                     (char *)0);
       if (mpRequestedHostName != NULL)
       {
          char *p_next_slash = strchr(mpRequestedHostName, '/');
          if (p_next_slash != NULL)
            *p_next_slash = '\0';
       }
    }
    else
    {
      // Approach #3: Punt. Use defined server name. 
      char        port_str[8];
      HttpdIpPort port = mpServer->Port();

      if (port == 80) 
        port_str[0] = '\0';
      else
        sprintf(port_str, ":%lu", (unsigned long )port);
 
      mpRequestedHostName = HttpdUtilities::StrVCat(mpServer->ServerHost(),
                                                    port_str,
                                                    (char *)0);
    }
  }

  return (mpRequestedHostName);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::ParseHttpVersion
//
// ACCESS:         public
//
// DESCRIPTION:    Figure out what version of HTTP we are using.
//
// RETURNS:        The protocol version of the request.
//
// REMARKS:        Assuming the version string has the required form,
//                 we store the version number in the same way that it
//                 is transmitted. This actually seems to result in 
//                 an extensible way of parsing the versions that does
//                 not consume a lot of code space.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Sep 23, 2001
// LAST MODIFIED:  Nov 16, 2001  BY: MYG (M1005: Fix bad cast)
//                 May  7, 2002      JC  (C0001: Add const)
//
//--------------------------------------------------------------------------
HttpdProtocolVersion HttpdRequest::ParseHttpVersion(const char *p_version)
{
  assert(p_version != NULL);

  const char     proto_prefix[] = "HTTP/";
  const size_t   pfx_len        = sizeof(proto_prefix) - 1;

  if (strncmp(p_version, proto_prefix, pfx_len) == 0)
  {
     // Skip the prefix.
     p_version += pfx_len; 

     // Find the low part.
     const char *p_dot = strchr(p_version, '.');
     if (p_dot != NULL)
     {
        char                 highbuf[16];
        size_t               highlen;
        HttpdProtocolVersion high, low;
        
        // Compute length of the high part and make sure it fits in the
        // scratch buffer.
        highlen = (size_t )(p_dot - p_version);
        if (highlen <= (sizeof(highbuf) - 1))
        {
          // Put the high part in the scratch buffer.
          strncpy(highbuf, p_version, highlen);
          highbuf[highlen] = '\0';

          // Get the low part.
          p_dot++;

          // Compute the version.
          high = (HttpdProtocolVersion )strtoul(highbuf, NULL, 0);
          low  = (HttpdProtocolVersion )strtoul(p_dot, NULL, 0);

          return (high * 256) + low;
        }
     }
  }

  return (HTTPv09);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::ResponseBodyNeeded
//
// ACCESS:         public
//
// DESCRIPTION:    Determine if a response body should be sent.
//
// RETURNS:        True if the response must include a body, false if
//                 no body should be sent.
//
// REMARKS:        This is derived as the common behavior between
//                 HTTP v1.0 and HTTP v1.1. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Sep 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdRequest::ResponseBodyNeeded(int status)
{
  if (strcmp(Method(), HttpdUtilities::mHeadRequest) != 0)
    switch (status)
    {
      case HTTPD_RESP_CONTINUE:
      case HTTPD_RESP_PROTOCOL:
      case HTTPD_RESP_NO_CONTENT:
      case HTTPD_RESP_NOT_MODIFIED:
         return (false);

      default:
         break;
    }

  return (true);  
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::ResponseHeader
//
// ACCESS:         public
//
// DESCRIPTION:    Send out an HTTP response code and status.
//
// REMARKS:        In addition, some common MIME headers that are 
//                 necessary for all responses are sent as well.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Aug 27, 2001  BY:  SPC - Additions to handle HTTP/0.9
//                 Aug 28, 2001  BY:  SPC - Abstract response codes
//                 Sep 23, 2001  BY:  MYG - Use response obj.
//                 Apr 14, 2002  BY:  AMH - A1001: Persistent connections
//
//--------------------------------------------------------------------------
void HttpdRequest::ResponseHeader(const HttpdResponseMsg &response)
{
  char        tm_buf[64];

  // For HTTP/0.9, no header is sent out.
  if (!mNeedHeaders)
    return;

  // Send back a date in the response. 
  HttpdUtilities::FormatTime(tm_buf, 
                             sizeof(tm_buf),
                             HttpdUtilities::mNetTimeFormat,
                             time(NULL));

  // Write out the response and first set of MIME headers.
  mSocket.Printf(
        "HTTP/1.0 %d %s\r\n"
        "Server: %s\r\n"
        "Date: %s\r\n"
        "Connection: %s\r\n",

        response.mStatus,
        response.mpName,
        mpServer->ServerName(),
        tm_buf,
        mLastReq ? "close" : "keep-alive");
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::ResponseBody
//
// ACCESS:         public
//
// DESCRIPTION:    Send out an HTML response message.
//
// REMARKS:        It is assumed that the MIME header sequence has
//                 NOT been completed before calling this routine.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Aug 27, 2001  BY:  SPC - Additions to handle HTTP/0.9 
//                 Sep 23, 2001  BY:  MYG - Use response obj.
//
//--------------------------------------------------------------------------
void HttpdRequest::ResponseBody(const HttpdResponseMsg &response, 
                                const char             *p_url)
{
  if (mNeedHeaders)
  {
    mSocket.Printf(
                    "Content-type: text/html\r\n"
                    "\r\n"
                  );
  }

  mSocket.Printf( 
           "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
           "<html><head><title>%d %s</title></head><body>\n", 
           response.mStatus,
           response.mpName);

  mSocket.Printf("%s", response.mpDescription);

  if (p_url != NULL)
    mSocket.Printf("<p>The page is <a href=\"%s\">here.</a>", p_url);

  mSocket.Printf("</body></html>\n\n");
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::Respond
//
// ACCESS:         public
//
// DESCRIPTION:    Send out an HTTP response code as well as the first
//                 set of MIME headers. 
//
// REMARKS:        Error handling is almost entirely done in this 
//                 method. For any response code other than success,
//                 a complete error page is also written here and the
//                 connection should be closed at that point. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Aug 27, 2001  BY:  SPC - Updated call to ResponseBody() 
//                 Sep 23, 2001  BY:  MYG - Use response obj.
//                 Sep 30, 2001  BY:  MYG - Send body only when expected.
//                 Apr 14, 2002  BY:  MYG - A1001: Persistent connections
//
//--------------------------------------------------------------------------
void HttpdRequest::Respond(int status)
{
  // Figure out an appropriate response.
  const HttpdResponseMsg &response = 
         HttpdResponseMsg::Find(status, Protocol());

  // If this is not a 200 response, close the connection
  // for HTTP/1.0 compatability.
  if (status != HTTPD_RESP_OK)
    SetLastReq();

  // Send back an HTTP response.
  ResponseHeader(response);

  // Write out a simple HTML error page. 
  if ((status != HTTPD_RESP_OK) && ResponseBodyNeeded(status))
    ResponseBody(response, NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRequest::Redirect
//
// ACCESS:         public
//
// DESCRIPTION:    Send out an HTTP redirect to a path or URI.
//
// REMARKS:        A relative path is not handled. The path should be
//                 normalized. If the location begins with a URI
//                 protocol it is taken to mean a redirect to possibly
//                 a different host.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Aug 27, 2001  BY:  SPC - Additions to handle HTTP/0.9
//                 Sep 23, 2001  BY:  MYG - Use response obj.
//                 Sep 30, 2001  BY:  MYG - Send body only when exptected
//                 Oct 19, 2001  BY:  MYG - Use Host header if there
//                 Jan 26, 2002  BY:  MYG (M1010: Host header contains port)
//                 Feb 11, 2002  BY:  MYG - Add lazy host name routine.
//
//--------------------------------------------------------------------------
void HttpdRequest::Redirect(int         status, 
                            const char *p_location)
{
  char       *p_must_free;

  // Figure out an appropriate response.
  const HttpdResponseMsg &response = 
      HttpdResponseMsg::Find(status, Protocol());

  // See if this is a full URI?
  if (HttpdUtilities::IsUriProtocol(p_location))
    p_must_free = NULL;
  else
  {
    const char *p_host_name = RequestedHostName();

    if (p_host_name == NULL)
      goto error;

    p_location  = 
    p_must_free = HttpdUtilities::StrVCat("http://", 
                                          p_host_name,
                                          p_location,
                                          (char *)0);
    if (p_location == NULL)
      goto error;
  } 

  // Close the connection on a redirect for HTTP/1.0 compatability.
  SetLastReq();

  // Send back a response.
  ResponseHeader(response);

  // Send out the location entry.
  if (mNeedHeaders)
    mSocket.Printf("Location: %s\r\n", p_location);

  // Write out HTML response (if necessary)
  if (ResponseBodyNeeded(status))
    ResponseBody(response, p_location);

  HttpdOpSys::Free(p_must_free);
  return;

error:
  Respond(HTTPD_RESP_SRV_ERROR);
}

