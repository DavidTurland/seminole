//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_ut.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Utility routines for Seminole.
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
//  002 |08/26/01| Added check for mailto: URLs.            | SPC  |
// -----+--------+------------------------------------------+------+---------
//  003 |09/30/01| Make request methods common.             | MYG  |
// -----+--------+------------------------------------------+------+---------
//  004 |10/06/01| Abstract memory allocation.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |11/05/01| Use GMT instead of local time zone.      | MYG  |
//      |        | Add extra EOS check to UriDecode.        |      |
// -----+--------+------------------------------------------+------+---------
//  006 |11/06/01| Case-insensitivity fix.                  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  007 |11/11/01| s/%Z/GMT in mNetTimeFormat[]; %Z is not  | AMH  |
//      |        | mandated by ANSI, and we know it's GMT.  |      |
// -----+--------+------------------------------------------+------+---------
//  008 |01/24/02| Add \0 byte for strftime.                | MYG  | M1008
// -----+--------+------------------------------------------+------+---------
//  009 |02/11/02| Don't walk off end of string in          | MYG  | M1012
//      |        | HasPrefix().                             |      |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

const char HttpdUtilities::mRoot[]          = "/";
const char HttpdUtilities::mNetTimeFormat[] = "%a, %d %b %Y %H:%M:%S GMT";
const char HttpdUtilities::mHeadRequest[]   = "HEAD";
const char HttpdUtilities::mGetRequest[]    = "GET";

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::StrVCat
//
// ACCESS:         public
//
// DESCRIPTION:    Concatenate multiple strings into a single string
//                 allocated from the heap.
//
// RETURNS:        A pointer to allocated storage that must be freed
//                 on success; NULL on failure.
//
// REMARKS:        This function is one of those classic string tricks
//                 that you should always have handy. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::StrVCat(const char *p_first, ...)
{
  va_list     va;
  size_t      len = 0;
  char       *p_head, *p_tail;
  const char *p_str;
  
  va_start(va, p_first);
  for (p_str = p_first; p_str != NULL; p_str = va_arg(va, char *))
    len += strlen(p_str);
  va_end(va);
  
  p_head = (char *)HttpdOpSys::Malloc(len + 1);
  if (p_head == NULL)
    return (NULL);
  
  p_tail = p_head;
  
  va_start(va, p_first);
  for (p_str = p_first; p_str != NULL; p_str = va_arg(va, char *))
  {
    while (*p_str != '\0')
      *p_tail++ = *p_str++;
  }
  va_end(va);
  
  *p_tail = '\0';
  
  return (p_head);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::GetLcExtension
//
// ACCESS:         public
//
// DESCRIPTION:    Obtain the extension of a POSIX file-name component
//                 in lower case. 
//
// RETURNS:        A pointer to the extension.
//
// REMARKS:        The input string is modified in place and the pointer
//                 returned points into that string. You should make a
//                 copy of the string for this routine if you need it.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::GetLcExtension(char *p_file_name)
{
  char *p_extension = strrchr(p_file_name, '.');

  if (p_extension != NULL)
  {
    char *p_lc_extension;

    // Lower-case the string in a private buffer.
    for(p_lc_extension = ++p_extension; 
       *p_lc_extension != '\0';
        p_lc_extension++)
      *p_lc_extension = tolower(*p_lc_extension);

    return (p_extension);
  }

  return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::StrChop
//
// ACCESS:         public
//
// DESCRIPTION:    A simple tokenizer for white-space delimited text.
//
// RETURNS:        A pointer to the next token.
//
// REMARKS:        The input string is modified in place. In addition,
//                 the input pointer is advanced past the token such 
//                 that successive calls to StrChop() will return
//                 successive tokens.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::StrChop(char *&p_string)
{
  char *p_front;

  for(p_front = p_string; 
      (*p_front != '\0') && isspace(*p_front); 
      p_front++);
  for(p_string = p_front; 
      (*p_string != '\0') && (!isspace(*p_string));
      p_string++);
  
  if (*p_string != '\0')
    *p_string++ = '\0';

  return (p_front);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::HtmlQuote
//
// ACCESS:         public
//
// DESCRIPTION:    Make a string safe for HTML-izing.
//
// RETURNS:        A pointer to the HTML-ized string or NULL on failure.
//                 The caller must free the returned buffer.
//
// REMARKS:        Basic HTML quoting is done here. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 20, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::HtmlQuote(const char *p_str)
{
  char *p_result = (char *)HttpdOpSys::Malloc((strlen(p_str) * 6) + 1);

  if (p_result == NULL)
    return (NULL);

  char *p_buf = p_result;
  while (*p_str != '\0')
  {
    switch (*p_str)
    {
      case '&': 
       strcpy(p_buf, "&amp;");
       p_buf += 5;
       break;

      case '<':
       strcpy(p_buf, "&lt;");
       p_buf += 4;
       break;

      case '>':
       strcpy(p_buf, "&gt;");
       p_buf += 4;
       break;

      case '\"':
       strcpy(p_buf, "&quot;");
       p_buf += 6;
       break;

      default:
       *p_buf++ = *p_str;
       break;
    }

    p_str++;
  }

  *p_buf = '\0';

  // Shrink the buffer since we over-allocated.
  return ((char *)HttpdOpSys::Realloc(p_result, strlen(p_result) + 1));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::StrCmpi
//
// ACCESS:         public
//
// DESCRIPTION:    Case-insensitive string compare.
//
// RETURNS:        >  0 if p_a is lexicographically greater than p_b
//                 == 0 if p_a is equal to p_b
//                 <  0 if p_a is lexicographically less than p_b
//
// REMARKS:
//
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdUtilities::StrCmpi(const char *p_a, const char *p_b)
{
  char ch_a, ch_b;

  for(;;)
  {
    ch_a = tolower(*p_a);
    ch_b = tolower(*p_b);

    if ((ch_a == 0) || (ch_a != ch_b))
      break;

    p_a++; p_b++;
  }

  return (ch_a - ch_b);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::Lookup
//
// ACCESS:         public
//
// DESCRIPTION:    Sorted table lookup (B-search)
//
// RETURNS:        A pointer to the value on success, NULL on failure.
//
// REMARKS:        Searching through tables is so commonly done that 
//                 we use this routine to do it whenever we need to.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  Nov  6, 2001  BY: MYG (Case insensitivity fix)
//
//--------------------------------------------------------------------------
const char *HttpdUtilities::Lookup(const HttpdPair *p_table,
                                   size_t           table_size,
                                   const char      *p_key,
                                   int            (*p_comp)(const char *p_a,
                                                            const char *p_b))
{
  int low  = 0;
  int high = (int )table_size - 1;
  
  while (low <= high)
  {
    int               mid, res;
    const HttpdPair  *p_entry;

    mid     = (low + high) / 2;
    p_entry = p_table + mid;
    res     = p_comp(p_entry->mpKey, p_key);

    if (res == 0) // Found it.
      return (p_entry->mpValue);
    else if (res > 0) // It's in the lower half.
      high = mid - 1;
    else // It's in the upper half.
      low  = mid + 1;
  }

  return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::Normalize
//
// ACCESS:         public
//
// DESCRIPTION:    Normalize a URI path specifier.
//
// RETURNS:        A pointer to the normalized path specifier on success,
//                 NULL on failure. The returned value must be free()ed
//                 by the caller.  
//
// REMARKS:        It is often necessary to convert a path consisting 
//                 of '.' and '..' referring to the current and previous
//                 containers (directories) respectively into a fully 
//                 qualified path specifier for many different URI
//                 handlers. This routine is very useful in these 
//                 situations.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::Normalize(const char *p_uri, const char *prefix)
{
  char *p_result;
  unsigned int depth = 0;
  char *p_tmp_uri;
  char *p_fn;
  
  /* Since we modify it, make a copy of the URI. */
  p_tmp_uri = p_fn = StrVCat(p_uri, (char *)0);
  if (p_tmp_uri == NULL)
    return (NULL);

  /* Get the result buffer. */
  p_result = (char *)HttpdOpSys::Malloc(strlen(p_fn) + strlen(prefix) + 16);
  if (p_result == NULL)
  {
    HttpdOpSys::Free(p_tmp_uri);
    return (NULL);
  }
  strcpy(p_result, prefix);
          
  /* Walk each path component. */
  for(;;)
  {
    char *p_eoc, save;
    
    /* Find next component. */
    p_eoc = strchr(p_fn, '/');
    if (p_eoc != NULL)
    {
      save = *p_eoc;
      *p_eoc = '\0';
    }
    
    /* Check the component for special cases. */
    if (strcmp(p_fn, "..") == 0)
    {
      if (depth > 0)
      {
        /* Chop off previous directory component. */
        char *p_last = strrchr(p_result, '/');
      
        if (p_last != NULL)
          p_last[1] = '\0';
        depth--;
      }
    }
    else if ((*p_fn != '\0') && (strcmp(p_fn, ".") != 0))
    {
      depth++;
      /* Make sure current results are '/' terminated. */
      if (*p_result != '\0')
      {
        char *p_path_eos = p_result + strlen(p_result) - 1;
  
        if (*p_path_eos != '/')
        {
          p_path_eos[1] = '/';
          p_path_eos[2] = '\0';
        }
      }
 
      /* Append new path component. */
      strcat(p_result, p_fn);
    }
    
    /* Out of path elements? */
    if (p_eoc == NULL)
      break;
    if (save != '\0')
      p_fn = p_eoc + 1;
    else
      break; 
  }
  
  HttpdOpSys::Free(p_tmp_uri);
  return (p_result);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::UriEncode
//
// ACCESS:         public
//
// DESCRIPTION:    URI encode a string, being safe about what we quote.
//
// RETURNS:        A pointer to an encoded string on success, NULL on
//                 failure. The string must be free()ed by the caller.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::UriEncode(const char *p_uri)
{
  char *p_result, *p_buf;

  /* Allocate space for a worst-case. */
  p_result = (char *)HttpdOpSys::Malloc((strlen(p_uri) * 3) + 1);
  if (p_result == NULL)
    return (NULL);

  p_buf = p_result;
  while (*p_uri != '\0')
  {
     if (*p_uri == ' ')
       *p_buf++ = '+';
     else if ((ispunct(*p_uri) || iscntrl(*p_uri) || isspace(*p_uri))
          &&  (strchr("-@*_.", *p_uri) == NULL))
       p_buf += sprintf(p_buf, "%%%-2.2X", *p_uri);
     else
      *p_buf++ = *p_uri;

     p_uri++;
  }

  *p_buf = '\0';
  return (p_result);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::UriDecode
//
// ACCESS:         public
//
// DESCRIPTION:    URI decode a string, unescaping any escaped entities
//                 along the way.
//
// RETURNS:        A pointer to a decoded string on success, NULL on
//                 failure. The string must be free()ed by the caller.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Nov  5, 2001  BY: MYG (Add extra EOS check)
//
//--------------------------------------------------------------------------
char *HttpdUtilities::UriDecode(const char *p_encoded)
{
  char  *p_result, *p_buf;

  p_result = (char *)HttpdOpSys::Malloc(strlen(p_encoded) + 1);
  if (p_result == NULL)
    return (NULL);

  p_buf = p_result;
  while (*p_encoded != '\0')
  {
    if (*p_encoded == '+')
    {
      *p_buf = ' ';
      p_encoded++;
    }
    else if (*p_encoded != '%')
      *p_buf = *p_encoded++;
    else
    {
       char  hex[3];

       hex[0] = *++p_encoded;
       hex[1] = *++p_encoded;
       if ((hex[0] == '\0') || (hex[1] == '\0'))
         break;
       hex[2] = '\0';

       *p_buf = (char )strtoul(hex, NULL, 16);
       p_encoded++;
    }

    p_buf++;
  }

  *p_buf = '\0';
  return (p_result);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::HasTrailingSlash
//
// ACCESS:         public
//
// DESCRIPTION:    Determine if a URI has a trailing slash.
//
// RETURNS:        True if the slash is present, false otherwise.
//
// REMARKS:        The input string is allowed to be empty.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdUtilities::HasTrailingSlash(const char *p_path)
{
  return ((*p_path != '\0') 
      &&  (*(p_path + strlen(p_path) - 1) == '/'));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::GetComponentPath
//
// ACCESS:         public
//
// DESCRIPTION:    Concatenate a filename to a URI path.
//
// RETURNS:        A pointer to the concatenated URI path on success,
//                 NULL on failure. The returned value must be free()ed
//                 by the caller.  
//
// REMARKS:        A useful string transformation is to join two 
//                 URI path components. However, the issue of 
//                 needing to inject additional slashes is always a 
//                 nuisance. This helper routine removes that chore.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 13, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
char *HttpdUtilities::GetComponentPath(const char *p_uri, 
                                       const char *p_filename)
{
  if (HasTrailingSlash(p_uri))
    return (StrVCat(p_uri, p_filename, (char *)0));
  else
    return (StrVCat(p_uri, mRoot, p_filename, (char *)0));
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::HasPrefix
//
// ACCESS:         public
//
// DESCRIPTION:    Determine if a string has a certain prefix (case
//                 insensitive).
//
// RETURNS:        True if the prefix is present, false otherwise.  
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Feb 11, 2002  BY:  MYG (M1012: Don't walk past end)
//
//--------------------------------------------------------------------------
bool  HttpdUtilities::HasPrefix(const char *p_str, const char *p_prefix)
{
  size_t len = strlen(p_prefix);

  while (len--)
  {
    if ((*p_str == '\0') || (tolower(*p_str) != tolower(*p_prefix)))
      return (false);

    p_str++;
    p_prefix++;
  }

  return (true);
}

static const char *uriProtocols[] =
{
  "http:",
  "mailto:",
  "ftp:",
  "file:",
  
  NULL
};

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::IsUriProtocol
//
// ACCESS:         public
//
// DESCRIPTION:    Determine if a URI contains a protocol.
//
// RETURNS:        True if the URI is absolute, false if not.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool  HttpdUtilities::IsUriProtocol(const char *p_uri)
{
  const char **pp_walk = uriProtocols;

  while (*pp_walk != NULL)
  {
    if (HasPrefix(p_uri, *pp_walk))
      return (true);

    pp_walk++;
  }

  return (false);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdUtilities::FormatTime
//
// ACCESS:         public
//
// DESCRIPTION:    Format a time_t value.
//
// REMARKS:        Given the non-reentrancy of the ANSI C time routines
//                 and their requirement for several steps of conversion
//                 for common idioms, this (possibly re-entrant) 
//                 utility routine should be used instead.  
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Nov  5, 2001  BY: MYG (Use GMT instead of local TZ)
//                 Jan 24, 2002      MYG (M1008: Add \0 byte)
//
//--------------------------------------------------------------------------
void HttpdUtilities::FormatTime(char       *p_buf, 
                                size_t      bufsz, 
                                const char *p_format,
                                time_t      t)
{
#ifdef HTTPD_HAVE_REENTRANT_LIB
  struct tm   tm[1];
  gmtime_r(&t, tm);
#else
  struct tm   *tm;
  tm = gmtime(&t);
#endif

  /* 
   * We always NUL terminate the string because ANSI C 
   * does not guarantee that it is NUL terminated.
   */
  strftime(p_buf, --bufsz, p_format, tm);
  p_buf[bufsz] = '\0';
}

