//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_cgi.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     CGI request processing
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct  6, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/06/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::FreeList
//
// ACCESS:         public
//
// DESCRIPTION:    Release the memory occupied by a list of CGI 
//                 parameters.
//
// REMARKS:        The list is no longer usable after calling this routine.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdCgiParameter::FreeList(HttpdCgiParameter *p_list)
{
   while (p_list != NULL)
   {
     HttpdOpSys::Free((void *)p_list->mPair.mpKey);
     HttpdOpSys::Free((void *)p_list->mPair.mpValue);

     p_list = p_list->mpNext;
   }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::Append
//
// ACCESS:         public
//
// DESCRIPTION:    Append a new parameter node onto a parameter list.
//
// REMARKS:        The list is stored as a two-element pointer array of
//                 head and tail. Before appending nodes to the 
//                 list, the head element must be set to NULL.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdCgiParameter::Append
  ( // PARAMETERS:
    HttpdCgiParameter  *p_param,  // Node to append.
    HttpdCgiParameter **pp_list   // Array of [head, tail] pointers.
  )
{
  p_param->mpNext = NULL;

  if (pp_list[0] == NULL) // First time insertion?
    pp_list[0] = pp_list[1] = p_param;
  else
  {
    pp_list[1]->mpNext = p_param;
    pp_list[1]         = p_param;
  }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::ParseParam
//
// ACCESS:         private
//
// DESCRIPTION:    Parse a name=value URI-encoded pair and append it
//                 onto a parameter list.
//
// RETURNS:        A pointer to the string to the next ('&' separated)
//                 pair, if there is one or NULL if no more pairs
//                 exist in the buffer.
//
//                 Success is indicated by the setting of "ok" to
//                 true.
//
// REMARKS:        The operation is considered a complete failure if
//                 there is insufficient memory to hold the data. 
//                 In the case of failure (ok == false), the entire
//                 operation should be aborted.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdCgiParameter::ParseParam(char               *p_param, 
                                    HttpdCgiParameter **pp_list,
                                    bool               &ok)
{
  char              *p_key, *p_value, *p_sep;
  HttpdCgiParameter *p_node;

  //----------------------------------------------------------------------
  // Step 1: Get the name component. 
  //----------------------------------------------------------------------

  p_sep = strchr(p_param, '=');
  if (p_sep == NULL)
    goto key_error;

  *p_sep++ = '\0';
 
  p_key = HttpdUtilities::UriDecode(p_param);
  if (p_key == NULL)
    goto key_error;

  //----------------------------------------------------------------------
  // Step 2: Get the value component. 
  //----------------------------------------------------------------------
     
  p_param = strchr(p_sep, '&');
  if (p_param != NULL)
    *p_param++ = '\0';

  p_value = HttpdUtilities::UriDecode(p_sep);
  if (p_value == NULL)
    goto value_error;

  //----------------------------------------------------------------------
  // Step 3: Append the node.
  //----------------------------------------------------------------------
 
  p_node = (HttpdCgiParameter *)HttpdOpSys::Malloc(sizeof(HttpdCgiParameter));
  if (p_node == NULL)
    goto node_error;

  p_node->mPair.mpValue = p_value;
  p_node->mPair.mpKey   = p_key;

  Append(p_node, pp_list);

  ok = true;
  return (p_param);

node_error:
   HttpdOpSys::Free(p_value);
value_error:
   HttpdOpSys::Free(p_key);
key_error:
   ok = false;
   return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::ParseUriString
//
// ACCESS:         public
//
// DESCRIPTION:    Remove CGI parameters from a URI string and 
//                 convert them into a list of HttpdCgiParameter
//                 objects.
//
// RETURNS:        A pointer to the head element of the parameter
//                 list if parameters were found. NULL is returned
//                 if there are no parameters.
//
// REMARKS:        The URI string is adjusted so that appended
//                 parameters are removed. It is the responsibility
//                 of the caller to free the returned list.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdCgiParameter *HttpdCgiParameter::ParseUriString(char *p_uri)
{
   char              *p_qmark = strrchr(p_uri, '?');

   HttpdCgiParameter *list[2];
   list[0] = NULL;

   if (p_qmark == NULL)
     return (NULL);
   else // We have some parameters.
   {
     *p_qmark++ = '\0'; // Chop them off.

     // Walk over the pairs.
     while ((p_qmark != NULL) && (*p_qmark != '\0'))
     {
        bool ok;
    
        p_qmark = ParseParam(p_qmark, list, ok);
        if (!ok)
          goto error;
     }
   }

   return (list[0]);

error:
   FreeList(list[0]);
   return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::ParsePostData
//
// ACCESS:         public
//
// DESCRIPTION:    Parse the data associated with a POST method
//                 that is not associated with the URI string
//                 (technically, parameters can be in both places).
//
// RETURNS:        A pointer to the head element of the parameter
//                 list if parameters were found. NULL is returned
//                 if there is an error. It is the responsibility
//                 of the caller to free the returned list.
//
// REMARKS:        The entire operation is considered a failure and
//                 no parameters are returned.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdCgiParameter *HttpdCgiParameter::ParsePostData(HttpdRequest *p_request)
{
   const char *p_content_type = p_request->Header("Content-type");

   // No encoding type? Can't continue from this.
   if (p_content_type == NULL)
     return (NULL);

   // Dispatch to appropriate encoding handler.
   if (strcmp(p_content_type, "application/x-www-form-urlencoded") == 0)
     return (HttpdCgiParameter::ParseUrlEncoded(p_request));
   else
     return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::ParseUrlEncoded
//
// ACCESS:         private
//
// DESCRIPTION:    Parse POST data in the 
//                 "application/x-www-form-url-encoded" format.
//
// RETURNS:        A pointer to the parameter list. It is the 
//                 responsibility of the caller to free the returned list.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdCgiParameter *HttpdCgiParameter::ParseUrlEncoded(HttpdRequest *p_request)
{
   size_t total_length, cur_length;

   //---------------------------------------------------------------------
   // From the headers: Get the length of the request.
   //---------------------------------------------------------------------

   const char *p_content_length = p_request->Header("Content-length");
   if (p_content_length == NULL)
     return (NULL);

   cur_length   = 0;
   total_length = (size_t )strtoul(p_content_length, NULL, 10);

   //---------------------------------------------------------------------
   // From the socket: Read in elements and append them to the list.
   //---------------------------------------------------------------------

   HttpdCgiParameter  *list[2];
   list[0] = NULL;

   char static_buf[256];

   while (cur_length < total_length)
   {
     char *p_buffer = static_buf;
     bool  ok, got_it;

     //-------------------------------------------------------------------
     // Step 1: Read in the next component.
     //-------------------------------------------------------------------

     got_it = ReadUntil(p_request->Socket(), 
                        '&', total_length - cur_length,
                        p_buffer, sizeof(static_buf));

     if (!got_it)
       goto error;

     cur_length += strlen(p_buffer) + 1; // Data + Separator ('&')

     //-------------------------------------------------------------------
     // Step 2: Consume the buffer.
     //-------------------------------------------------------------------

     (void )ParseParam(p_buffer, list, ok);

     if (p_buffer != static_buf)
       HttpdOpSys::Free(p_buffer);

     //-------------------------------------------------------------------
     // Step 3: Did that work?
     //-------------------------------------------------------------------

     if (!ok)
       goto error;
   }

   return (list[0]);

error:
   FreeList(list[0]);
   return (NULL);   
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdCgiParameter::ReadUntil
//
// ACCESS:         private
//
// DESCRIPTION:    This is a helper function for POST CGI data.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        This function reads up to the specified amount
//                 of data (max_len) from the socket until either
//                 the maximum amount is reached or the termination
//                 character (term) stops the input operation. 
//
//                 To prevent fragmentation, normally a statically
//                 allocated buffer is used as the destination.
//                 Should the input exceed the size of the 
//                 statically allocated buffer (bufsz), a new
//                 dynamically allocated buffer is used. 
//
//                 On a successful return from this routine, p_buffer
//                 may be modified to point to a dynamic buffer. The 
//                 calling routine should check for this and free
//                 the buffer if necessary.
//
//                 This contorted logic helps prevent fragmentation
//                 because we intend to build a list out of the read data
//                 and temporary buffers would cause severe fragmentation.
//                 We thus avoid this for most of the normal cases.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdCgiParameter::ReadUntil(HttpdSocket &sock, 
                                  char         term,
                                  size_t       max_len,
                                  char       *&p_buffer,
                                  size_t       bufsz)
{
  char    byte;
  size_t  n = 0;
  bool    something = false;
  bool    ownbuffer = false;

  assert(bufsz > 2);
  assert(p_buffer != NULL);

  while (n < max_len)
  {
    ssize_t  s = sock.ReadN(&byte, 1);
    if (s < 1)
      return (something);

    something = true;

    if (byte == term)
      break;

    if (n == (bufsz - 1))
    {
      bufsz += HTTPD_MAX_INPUT_LINE;

      // Time to grow the buffer...
      // There are two main cases here:
      //
      //   (1) The provided statically-allocated buffer is 
      //       overflowed for the first time. 
      //
      //
      //   (2) The already allocated buffer is full and 
      //       needs to be expanded.
      //
      if (ownbuffer)
      {
        char *p_new_buf = (char *)HttpdOpSys::Realloc(p_buffer, bufsz);
        if (p_new_buf == NULL)
          return (false);

        p_buffer = p_new_buf;
      }
      else
      {
        char *p_new_buf = (char *)HttpdOpSys::Malloc(bufsz);
        if (p_new_buf != NULL)
          return (false);
        memcpy(p_new_buf, p_buffer, n);
        p_buffer = p_new_buf;
      }
    }

    p_buffer[n++] = byte;
  }

  p_buffer[n] = '\0';
  return (true);
}

