//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_hand.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     HTTP request handler (abstract)
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
//  002 |10/06/01| Updated for CGI.                         | MYG  |
// -----+--------+------------------------------------------+------+---------
//  003 |10/28/01| Use hierarchical path parsing.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdHandler:~HttpdHandler
//
// ACCESS:         public
//
// DESCRIPTION:    Virtual destructor for handlers
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdHandler::~HttpdHandler()
{}

//---------------------------------------------------------------------------
//
// NAME:           HttpdHandler::Install
//
// ACCESS:         protected
//
// DESCRIPTION:    Install this handler in the handler chain. 
//
// REMARKS:        This method inserts this handler into the handler
//                 list. It is called from the Httpd class (a friend)
//                 such that the list is managed from there. 
//
//                 The handler list is an interesting beast, it is
//                 sorted by decreasing string length order. This is
//                 so that the most specific matches are conducted 
//                 first. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdHandler::Install(HttpdHandler *&p_old_head)
{
  size_t         len     = strlen(mpPrefix);
  HttpdHandler **pp_prev = &p_old_head; 
  HttpdHandler  *p_walk;

  for (p_walk = p_old_head; p_walk != NULL; p_walk = p_walk->mpNext)
  {
    if (len > strlen(p_old_head->mpPrefix))
      break;

    pp_prev = &(p_old_head->mpNext);
  }

  mpNext = (*pp_prev);
  (*pp_prev) = this;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdHandler::IsMe
//
// ACCESS:         public
//
// DESCRIPTION:    Determine if this incoming request is for us. 
//
// RETURNS:        A pointer to the handler-specific portion of the path
//                 if this request is indeed for us or NULL if the 
//                 request is not for us.
//
// REMARKS:        This strange interface is simply to save on code
//                 space. If things weren't so tight, I would probably
//                 have done them differently.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdHandler::IsMe(HttpdRequest *p_request)
{
  size_t      len = strlen(mpPrefix);
  char       *p_path = p_request->Path();

  if (strncmp(p_path, mpPrefix, len) == 0)
    return (p_path + len);
  else
    return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdHandler::IsMyPath
//
// ACCESS:         public
//
// DESCRIPTION:    Determine if this incoming request is for us when
//                 we have a hierarchical URI.
//
// RETURNS:        A pointer to the handler-specific portion of the path
//                 if this request is indeed for us or NULL if the 
//                 request is not for us.
//
// REMARKS:        This routine is used when you have a handler covering
//                 for a hierarchical directory structure. It is 
//                 different from the IsMe() function because of the 
//                 following pathological case: 
//
//                 Assume that Seminole is set up with the following 
//                 prefix maps:
//
//                   /         => /dir1 (via HttpdFileHandler)
//                   /seminole => /dir2 (via HttpdFileHandler)
//
//                 Assume these files in dir1:
//
//                   seminole_logo1.jpg
//                   seminole_logo2.jpg
//
//                 Assume that dir2 contains an index.html.
//
//                 Now, we want to browse the index.html file, so we give 
//                 a URL of http://coffeemaker.house.net/seminole and, indeed,
//                 the handler invoked is that providing service for /dir2. 
//                 However, assuming we want the logo files, if we attempted 
//                 to get /seminole_logo1.jpg, the other (/seminole) is
//                 invoked with a direct "IsMe" match. 
//
//                 Often, this is not what we want.  
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 28, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdHandler::IsMyPath(HttpdRequest *p_request, const char sep)
{
  size_t      len = strlen(mpPrefix);
  char       *p_path = p_request->Path();

  // If the prefix ends with a '/', then don't include that in the
  // comparison.
  if ((len > 0) && (p_path[len - 1] == sep))
    len--;

  // Now, check for the prefix (if there is any left after the 
  // above logic). 
  if ((len == 0) || (strncmp(p_path, mpPrefix, len) == 0))
  {
    p_path += len;

    // It had better be the end of the string or followed by the 
    // path separator. 
    if (*p_path == sep)
      return (p_path + 1);
    else if (*p_path == '\0')
      return (p_path);
  }

  return (NULL);
}

