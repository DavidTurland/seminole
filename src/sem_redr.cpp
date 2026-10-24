//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_redr.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Generic URI redirection handler for Seminole HTTPD
//
// AUTHOR:          A. Hoerter
//
// CREATION DATE:   Aug 26, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |08/26/01| File creation.                           | AMH  |  
// -----+--------+------------------------------------------+------+---------
//  002 |09/23/01| Add HTTP status arg, for different types | AMH  |
//      |        | of redirects.                            |      |
// -----+--------+------------------------------------------+------+---------
//  003 |10/06/01| Abstract memory allocation.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdRedirector::HttpdRedirector
//
// ACCESS:         public
//
// DESCRIPTION:    Construct & configure an HttpdRedirector object.
//               
// AUTHOR:         A. Hoerter (adapted from M. Grosberg's HttpdFileHandler)
// CREATED:        Aug 26, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdRedirector::HttpdRedirector(const char *p_prefix, const char *p_newuri,
                                 int status)
{
  mpPrefix    = HttpdUtilities::StrVCat(p_prefix, (char *)0);
  mpNewUri    = HttpdUtilities::StrVCat(p_newuri, (char *)0);
  mStatusCode = status;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRedirector::~HttpdRedirector
//
// ACCESS:         public
//
// DESCRIPTION:    Release any resources held by the redirector.
//
// AUTHOR:         A. Hoerter (adapted from M. Grosberg's HttpdFileHandler)
// CREATED:        Aug 26, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdRedirector::~HttpdRedirector()
{
  HttpdOpSys::Free((char *)mpPrefix);
  HttpdOpSys::Free(mpNewUri);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdRedirector::Handle
//
// ACCESS:         public
//
// DESCRIPTION:    Dispatcher procedure.
//
// RETURNS:        True if the request was handled, false if not.
//
// REMARKS:        This method determines if the request is appropriate
//                 for the handler. If so, the request is processed 
//                 with the assistance of the utility routines of this
//                 class.
//
// AUTHOR:         A. Hoerter (adapted from M. Grosberg's HttpdFileHandler)
// CREATED:        Aug 26, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdRedirector::Handle(HttpdRequest *p_request)
{
  const char *p_req_path = IsMe(p_request);

  if (p_req_path != NULL)
  {
    p_request->Redirect(mStatusCode, mpNewUri);
    return (true);
  }
  else
   return (false); // We don't handle this...
}


