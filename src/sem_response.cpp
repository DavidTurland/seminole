//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_response.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Routines for converting HTTP status codes to human
//                  readable text strings.
//
// AUTHOR:          S. Conner
//
// CREATION DATE:   Aug 28, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |08/28/01| File creation.                           | SPC  |
// -----+--------+------------------------------------------+------+---------
//  002 |09/23/01| Protocol response negotiation.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  003 |01/11/02| Make ResponseMsg initializable for some  | MYG  | M1004
//      |        | C++ compilers.                           |      |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

static const char nullstr[1] = { '\0' };

static const HttpdResponseMsg response_table[] = 
{
  { 100 , "Continue", nullstr , HTTPv11 },
  { 101 , "Switching Protocols", nullstr , HTTPv11 },
  { 200 , "OK"                              , nullstr , HTTPv10 } ,
  { 201 , "Created"                         , nullstr , HTTPv10 } ,
  { 202 , "Accepted"                        , nullstr , HTTPv10 } ,
  { 203 , "Non-Authoritative Information"   , nullstr , HTTPv11 } ,
  { 204 , "No Content"                      , nullstr , HTTPv10 } ,
  { 205 , "Reset Content"                   , nullstr , HTTPv11 } ,
  { 206 , "Partial Content"                 , nullstr , HTTPv11 } ,
  { 300 , "Multiple Choices"                , nullstr , HTTPv10 } ,
  { 301 , "Moved Permanently"               , "The document has moved.\n" , HTTPv10 } ,
  { 302 , "Found"                           , "The document has moved.\n" , HTTPv10 } ,
  { 303 , "See Other"                       , nullstr , HTTPv11 } ,
  { 304 , "Not Modified"                    , nullstr , HTTPv10 } ,
  { 305 , "Use Proxy"                       , nullstr , HTTPv11 } ,
    // 306 not used
  { 307 , "Temporary Redirect"              , nullstr , HTTPv11 } ,
  { 400 , "Bad Request"                     , "The request was invalid.\n" , HTTPv10 } ,
  { 401 , "Unauthorized"                    , nullstr , HTTPv10 } ,
  { 402 , "Payment Required"                , nullstr , HTTPv11 } ,
  { 403 , "Forbidden"                       , "Permission denied for object.\n" , HTTPv10 } ,
  { 404 , "Not Found"                       , "The document requested was not found on this server.\n" , HTTPv10 } ,
  { 405 , "Method Not Allowed"              , nullstr , HTTPv11 } ,
  { 406 , "Not Acceptable"                  , nullstr , HTTPv11 } ,
  { 407 , "Proxy Authentication Required"   , nullstr , HTTPv11 } ,
  { 408 , "Request Time-out"                , nullstr , HTTPv11 } ,
  { 409 , "Conflict"                        , nullstr , HTTPv11 } ,
  { 410 , "Gone"                            , nullstr , HTTPv11 } ,
  { 411 , "Length Required"                 , nullstr , HTTPv11 } ,
  { 412 , "Precondition Failed"             , nullstr , HTTPv11 } ,
  { 413 , "Request Entity Too Large"        , nullstr , HTTPv11 } ,
  { 414 , "Request-URI Too Large"           , nullstr , HTTPv11 } ,
  { 415 , "Unsupported Media Type"          , nullstr , HTTPv11 } ,
  { 416 , "Requested range not satisfiable" , nullstr , HTTPv11 } ,
  { 417 , "Expectation Failed"              , nullstr , HTTPv11 } ,
  { 500 , "Internal Server Error"           , nullstr , HTTPv10 } ,
  { 501 , "Not Implemented"                 , "The requested method is not implemented by this server.\n" , HTTPv10 } ,
  { 502 , "Bad Gateway"                     , nullstr , HTTPv10 } ,
  { 503 , "Service Unavailable"             , nullstr , HTTPv10 } ,
  { 504 , "Gateway Time-out"                , nullstr , HTTPv11 } ,
  { 505 , "HTTP Version not supported"      , nullstr , HTTPv11 } ,
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdResponseMsg::Find
//
// ACCESS:         N/A
//
// DESCRIPTION:    Finds the correct table entry for the given HTTP
//                 status code.
//
// RETURNS:        The table entry to the HTTP status code.
//
// REMARKS:        If the appropriate response can not be found, it is
//                 translated to a server error. This is the correct 
//                 behavior as it implies a broken handler. 
//
// AUTHOR:         S. Conner
// CREATED:        Aug 28, 2001
// LAST MODIFIED:  Jan 11, 2002  BY:  MYG (M1004: Use global static)
//
//--------------------------------------------------------------------------
const struct HttpdResponseMsg &HttpdResponseMsg::Find(int status)
{
   int low  = 0;
   int high = HTTPD_NUMELEM(response_table) - 1;

   while (low <= high)
   {
      int                     mid   = (low + high) / 2;
      const HttpdResponseMsg *p_msg = response_table + mid;

      if (status == p_msg->mStatus) // Found it.
        return (*p_msg);
      else if (p_msg->mStatus > status) // It's in the ower half.
        high = mid - 1;
      else // It's in the upper half.
        low  = mid + 1;
   }

   return (Find(HTTPD_RESP_SRV_ERROR));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdResponseMsg::Find
//
// ACCESS:         N/A
//
// DESCRIPTION:    Finds the correct table entry for the given HTTP
//                 status code and protocol version.
//
// RETURNS:        The table entry most appropriate for the
//                 intended response.
//
// REMARKS:        Protocol negotiation is surprisingly this simple
//                 in most (non-redirect) cases.
//
// AUTHOR:         M. Grosberg
// CREATED:        Sep 23, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
const HttpdResponseMsg &HttpdResponseMsg::Find
  ( // PARAMETERS:
    int                  resp, 
    HttpdProtocolVersion vers
  )
{
  const HttpdResponseMsg &result = Find(resp);

  if (result.mVersion > vers)
  {
     // Okay, the response we want to generate will not be understood by
     // the client. Our only choice at this point is to map the error to
     // the most general error in the group and hope that the client 
     // takes the correct action. 
     return Find(resp - (resp % 100));
  }
  else
    return (result);
}

