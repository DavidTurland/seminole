//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_fs.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     File Service handler for Seminole HTTPD
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Mar 14, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |03/14/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |09/30/01| Made request method strings common.      | MYG  |
// -----+--------+------------------------------------------+------+---------
//  003 |10/06/01| Code-space optimization.                 | MYG  |
// -----+--------+------------------------------------------+------+---------
//  004 |10/06/01| Abstract memory allocation.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  005 |10/18/01| Handle If-Modified-Since optimizations.  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  006 |10/23/01| New filesystem abstraction.              | MYG  |
//      |        | Also added feature selectability via the |      |
//      |        | C preprocessor.                          |      |
// -----+--------+------------------------------------------+------+---------
//  007 |10/28/01| Use hierarchical path parsing.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  008 |11/13/01| Don't send headers for HTTP/0.9.         | AMH  | A1000
// -----+--------+------------------------------------------+------+---------
//  009 |01/18/02| Use strftime and not time_t.             | MYG  | M1007
// -----+--------+------------------------------------------+------+---------
//  010 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  011 |01/26/02| Add separator to prefix if needed.       | MYG  | M1009
// -----+--------+------------------------------------------+------+---------
//  012 |02/05/02| Provide a way to force files to be       | MYG  |
//      |        | designated for internal use only.        |      |
// -----+--------+------------------------------------------+------+---------
//  013 |04/14/02| Add persistent connections.              | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  014 |05/02/02| Change free to use OS interface.         | MYG  | M1016
// -----+--------+------------------------------------------+------+---------
//  015 |06/24/02| Change redirect type.                    | MYG  | M1024
// -----+--------+------------------------------------------+------+---------
//  016 |07/11/02| Handle errors better on persistent       | MYG  | M1031
//      |        | sockets.                                 |      |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::HttpdFileHandler
//
// ACCESS:         public
//
// DESCRIPTION:    Initialize the filehandler to a prefix in URL space
//                 that is mapped to a region in file system space.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Jan 19, 2002  BY:  MYG (New filesystem abstraction)
//
//--------------------------------------------------------------------------
HttpdFileHandler::HttpdFileHandler(HttpdFileSystem  *p_filesys,
                                   const char       *p_root, 
                                   const char       *p_prefix)
  : mpFileSystem(p_filesys)
{
  mpRootPath = HttpdUtilities::StrVCat(p_root, (char *)0);
  mpPrefix   = HttpdUtilities::StrVCat(p_prefix, (char *)0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::~HttpdFileHandler
//
// ACCESS:         public
//
// DESCRIPTION:    Release any resources held by the file handler.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdFileHandler::~HttpdFileHandler()
{
  HttpdOpSys::Free(mpRootPath);
  HttpdOpSys::Free((char *)mpPrefix);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::ErrorHandler
//
// ACCESS:         public
//
// DESCRIPTION:    Report an operating system error back to the request.
//
// REMARKS:        If file system operations fail during file serving
//                 and the result has not begun being pushed out yet 
//                 this method will generate an appropriate error
//                 message and terminate the request. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Oct  6, 2001  BY:  MYG (Space optimization)
//                 Oct 28, 2001  BY:  AMH (Updated constants in HttpdOpSys)
//
//--------------------------------------------------------------------------
void HttpdFileHandler::ErrorHandler(HttpdRequest *p_request, int xerrno)
{
  int status;

  switch (xerrno) 
  {
    case HttpdOpSys::ERR_NOTFOUND: status = HTTPD_RESP_NOT_FOUND; break;
    case HttpdOpSys::ERR_SYSPERM:  status = HTTPD_RESP_FORBIDDEN; break;
    default:                       status = HTTPD_RESP_SRV_ERROR; break;
  }

  p_request->Respond(status);
}

#if defined(HTTPD_INC_MODIFIED_SINCE)

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::NeedToSendOut
//
// ACCESS:         protected
//
// DESCRIPTION:    Determine if the request is for unmodified content.
//
// RETURNS:        True if the result should include content, false
//                 if not.
//
// REMARKS:        This routine checks the If-Modified-Since header
//                 and parses out the timestamp as well as any
//                 additional clauses that may require resending. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 18, 2001  X
// LAST MODIFIED:  May  2, 2002  BY:  MYG (M1016: Fix call to free)
//
//--------------------------------------------------------------------------
bool HttpdFileHandler::NeedToSendOut(HttpdRequest        *p_request,
                                     const HttpdFileInfo &info)
{
   static const char length_clause[] = "length=";

   bool        result = true;
   const char *p_if_modified_since = p_request->Header("If-Modified-Since");

   if (p_if_modified_since == NULL)
     return (true); // Always send if no If-* header.

   char *p_copy;

   // It is possible that we also have a list of clauses following
   // the timestamp. While this demonstrates bad protocol design,
   // we attempt to handle what we understand if present.
   if (strrchr(p_if_modified_since, ';') != NULL)
   {
     char       *p_sep, *p_pair;

     p_copy = HttpdUtilities::StrVCat(p_if_modified_since, (char *)0);
     if (p_copy == NULL)
       goto done;

     p_sep = strrchr(p_copy, ';');
     assert(p_sep != NULL);

     *p_sep++ = '\0';
 
     // While chopped string is not empty - parse token.
     while ((p_pair = HttpdUtilities::StrChop(p_sep))[0] != '\0')
     {
       if (strncmp(p_pair, length_clause, sizeof(length_clause) - 1) == 0)
       {
         if (strtoul(p_pair + sizeof(length_clause) - 1, NULL, 0) 
               != info.Size())
           goto done;    
       } 
     }
   }
   else
    p_copy = (char *)p_if_modified_since;

   //--------------------------------------------------------------------
   // If we made it here, then it means that all of the clauses did not
   // cause a decision to be reached. Now we perform a time comparison
   // on the file object to determine if the client's cached copy is
   // acceptable.
   //--------------------------------------------------------------------

   HttpdTimeStamp if_modified_since, file_mtime;

   // Get the times.
   if (file_mtime.Convert(info.LastModificationTime()))
   {
     if (if_modified_since.Parse(p_copy))
       result = (file_mtime.Compare(&if_modified_since) > 0);
   }

done:
   // If we allocated a buffer, free it.
   if (p_copy != p_if_modified_since)
     HttpdOpSys::Free(p_copy);

   return (result);
}

#endif

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::SendFile
//
// ACCESS:         protected
//
// DESCRIPTION:    Deliver the contents of a stdio stream to the 
//                 client. 
//
// REMARKS:        This routine is used to deliver a file with headers.
//                 The HttpdFile::PushToSink() method is then called to
//                 write the actual file data.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Oct 18, 2001  BY:  MYG (Added I-M-S processing)
//                 Jan 14, 2002       MYG (A1001: Content Sink)
//                 Jan 18, 2002       MYG (M1007: Use strftime)
//                 Jan 19, 2002       MYG (New filesystem abstraction)
//                 Jul 11, 2002       MYG (M1031: Handle error better)
//
//--------------------------------------------------------------------------
void HttpdFileHandler::SendFile(const char          *p_path, 
                                HttpdRequest        *p_request,
                                const HttpdFileInfo &info,
                                bool                 is_get)
{
  bool            need_to_send_out;

  // If we need headers, send them out!
  if (p_request->NeedHeaders())
  {
    const char *p_content_type;
    char        tm[64];

    // Get the MIME type from the extension.
    p_content_type = info.MimeType();
    if (p_content_type == NULL)
    {
      p_request->Respond(HTTPD_RESP_SRV_ERROR);
      return;
    }
    
    // See if we even need to send the content.
#if defined(HTTPD_INC_MODIFIED_SINCE)
    need_to_send_out = NeedToSendOut(p_request, info);
#else
    need_to_send_out = true;
#endif

    // Send out the result headers.
    strftime(tm, sizeof(tm) - 1,  
             HttpdUtilities::mNetTimeFormat,
             info.LastModificationTime());
    tm[sizeof(tm) - 1] = '\0';

    p_request->Respond(need_to_send_out 
                         ? HTTPD_RESP_OK 
                         : HTTPD_RESP_NOT_MODIFIED);

    p_request->Socket().Printf("Content-length: %t\r\n"
                               "Last-modified: %s\r\n"
                               "Content-type: %s\r\n\r\n",
                               info.Size(),
                               tm,
                               p_content_type);
  }
  else
    need_to_send_out = true; // HTTP v0.9 always sends.

  // Do we need to actually send the content? 
  if (is_get && need_to_send_out)
  {
    HttpdFile *p_file;

    // Open the file.
    if (info.FileSystem()->Open(info, 
                                HttpdFileSystem::FILE_READ_ONLY, 
                                p_file) != 0)
    {
      p_request->SetLastReq();
      return;
    }

    // Send the contents.
    if (p_file->PushToSink(&p_request->Socket()))
      p_request->SetLastReq();

    p_file->Close();
    return;
  }
}

#if defined(HTTPD_INC_DIRECTORY_LISTS)

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::SizeToString
//
// ACCESS:         public
//
// DESCRIPTION:    Print the size of a file in a compact form. 
//
// REMARKS:        This routine attempts to turn a file size into a 
//                 pleasing approximation that a human should prefer. 
//
//                 The buffer pointed to by the p_out parameter must
//                 be large enough to contain the results.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdFileHandler::SizeToString(char *p_out, unsigned long size)
{
  if (size >= 1000000UL)
  {
     sprintf(p_out, "%5lu",
             size / 100000UL);

     // Make decimal.
     p_out[7] = '\0';
     p_out[6] = 'M';
     p_out[5] = p_out[4];
     p_out[4] = '.';
  } 
  else if (size >= 1024UL)
  {
     sprintf(p_out, "%5lu", size / 100UL);

     // Make decimal.
     p_out[7] = '\0';
     p_out[6] = 'K';
     p_out[5] = p_out[4];
     p_out[4] = '.';
  }
  else
     sprintf(p_out, "   %4lu", size);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::ListEntry
//
// ACCESS:         protected
//
// DESCRIPTION:    Describe a filesystem object attractively in HTML.
//
// RETURNS:        True if the next file should be processed, false in 
//                 the face of a fatal error.
//
// REMARKS:        When we must build a directory listing we use this
//                 helper to describe each directory entry. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Jan 18, 2002  BY:  MYG (M1007: Use strftime)
//                 Jan 22, 2002       MYG (New filesystem abstraction)
//                 Apr 14, 2002       MYG (A1001: Persistent connections)
//
//--------------------------------------------------------------------------
bool HttpdFileHandler::ListEntry(HttpdWritable       *p_out,
                                 const char          *p_filename, 
                                 const HttpdFileInfo &info)
{
  char         size[32];
  char         date_str[48];
  char        *p_encoded;
  bool         is_dir = info.IsDir();


  // Format the entry. 
  if (is_dir)
    strcpy(size, "   (DIR)");
  else
    SizeToString(size, info.Size());
  strftime(date_str, sizeof(date_str) - 1,
           "%b-%d-%y %H:%M:%S",
           info.CreationTime());
  date_str[sizeof(date_str) - 1] = '\0';

  p_encoded = HttpdUtilities::UriEncode(p_filename);
  if (p_encoded == NULL)
    return (true);

  // Quote the filename. It's HTML.
  char *p_html_filename = HttpdUtilities::HtmlQuote(p_filename);
  if (p_html_filename == NULL)
  {
    HttpdOpSys::Free(p_encoded);
    return (true);
  }

  // Compute padding space.
  size_t name_len = strlen(p_filename);
  if (name_len >= 40)
    name_len = 0;
  else
    name_len = 40 - name_len;

  int xerror = p_out->Printf(
    " <a href=\"%s%s\">%s</a>%f %s %s\n",
    p_encoded,
    is_dir ? HttpdUtilities::mRoot : "",
    p_html_filename,
    name_len, ' ',
    date_str,      
    size);

  HttpdOpSys::Free(p_encoded);
  HttpdOpSys::Free(p_html_filename);
  return (xerror == 0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::ListParentDir
//
// ACCESS:         protected
//
// DESCRIPTION:    Add an "up" link to the file listing so that clients
//                 can go back to the parent directory easily.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 16, 2001
// LAST MODIFIED:  Apr 14, 2002  BY:  MYG (A1001: Persistent connections)
//
//--------------------------------------------------------------------------
void HttpdFileHandler::ListParentDir(HttpdRequest  *p_request,
                                     HttpdWritable *p_out)
{
  // No need to display parent for root of URI.
  if (strcmp(p_request->Path(), mpPrefix) == 0)
    return;

  // Compute a suffix.
  const char *p_suffix = "/..";
  if (HttpdUtilities::HasTrailingSlash(p_request->Path()))
    p_suffix++;

  // Make a non-normalized URI.
  char *p_strange = HttpdUtilities::StrVCat(p_request->Path(), 
                                            p_suffix, 
                                            (char *)0);
  if (p_strange == NULL)
    return;

  // Now, normalize it.
  char *p_normal = HttpdUtilities::Normalize(p_strange,
                                             HttpdUtilities::mRoot);
  
  // Now, submit it.
  if (p_normal != NULL)
    p_out->Printf(" <a href=\"%s\"><b>Up</b></a>\n\n",
                  p_normal);

  HttpdOpSys::Free(p_strange);
  HttpdOpSys::Free(p_normal);  
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::DirectoryBody
//
// ACCESS:         protected
//
// DESCRIPTION:    Generate the directory listing content to a
//                 writable object.
//
// REMARKS:        On errors we "try our best" there isn't much else we
//                 can really do otherwise.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Apr 13, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdFileHandler::DirectoryBody(HttpdRequest   *p_request,
                                     HttpdDirectory *p_dir, 
                                     HttpdWritable  *p_out,
                                     const char     *p_directory)
{
  const char *p_dirname = strrchr(p_directory, '/');

  if (p_dirname == NULL)
    p_dirname = p_directory;
  else
    p_dirname++;

  ListParentDir(p_request, p_out);

  p_out->Printf(
       "<html><head><title>Directory listing for %s"
       "</title></head><body>\n"
       "<H1>Index of %s</H1>\n"
       "<PRE> Name                                     "
       "Last modified        Size\n"
       "<HR>",
       p_dirname, p_dirname);

  // Show the directory contents.
  while (p_dir->Next() == 0)
  {
    char          *p_full_path;
    const char    *p_dname;
    HttpdFileInfo  file_info;

    p_dname = p_dir->Name();

    // Omit the hidden files.    
    if (p_dname[0] == '.')
      continue;

    // Get the path to the file.
    p_full_path = HttpdUtilities::GetComponentPath(p_directory, 
                                                   p_dname);
    if (p_full_path == NULL)
      continue;

    // Get some info on the file.
    int res = mpFileSystem->FileInfo(p_full_path, file_info);
    HttpdOpSys::Free(p_full_path);
    if (res != 0)
      continue;

    // To avoid buffering up lots of dead data, if we get a remote RST,
    // the printing should fail and we terminate early. 
    if (!ListEntry(p_out, p_dname, file_info))
      break;
  }

  p_out->Printf("</pre></body></html>");
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::ListDirectory
//
// ACCESS:         protected
//
// DESCRIPTION:    Given a file system directory entry, list the contents
//                 for a get request in a pretty-printed HTML format.
//
// REMARKS:        The generated HTML tries to take a very clean, 
//                 low-impact style. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Nov 12, 2001  BY:  AMH (Check NeedHeaders for HTTP/0.9)
//                 Jan 18, 2002       MYG (M1007: Use strftime)
//                 Jan 22, 2002       MYG (New filesystem abstraction)
//                 Apr 14, 2002       MYG (A1001: Persistent connections)
//                 Jul 11, 2002       MYG (M1031: Handle errors better)
//
//--------------------------------------------------------------------------
void HttpdFileHandler::ListDirectory(const char           *p_directory, 
                                     HttpdRequest         *p_request,
                                     const char           *p_decoded,
                                     const HttpdFileInfo  &info,
                                     bool                  is_get)
{
  HttpdDirectory   *p_dir;
  char              tm[64];
  HttpdContentSink  body;

  int xerrno = info.FileSystem()->Open(info, p_dir);
  if (xerrno != 0)
  {
    ErrorHandler(p_request, xerrno);
    return;
  }

  if (is_get)
    DirectoryBody(p_request,
                  p_dir, 
                  &body,
                  p_directory);

  // If headers are allowed, print them.  
  if (p_request->NeedHeaders())
  {
    // Send out a subset of response headers. 
    strftime(tm, sizeof(tm) - 1,
             HttpdUtilities::mNetTimeFormat,
             info.LastModificationTime());
    tm[sizeof(tm) - 1] = '\0';

    p_request->Respond(HTTPD_RESP_OK);
    if (is_get)
      p_request->Socket().Printf("Content-length: %t\r\n",
                                 body.ContentLength());
    p_request->Socket().Printf("Content-type: text/html\r\n"
                               "Last-modified: %s\r\n\r\n",
                               tm);
  }

  if (is_get)
  {
    if (body.SendData(&p_request->Socket()))
      p_request->SetLastReq();
  }

  p_dir->Close();
}

#endif

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::DoDirectory
//
// ACCESS:         public
//
// DESCRIPTION:    Directory handling. 
//
// REMARKS:        In general it seems that HTTP treats directories
//                 as a special case, issuing redirects for 
//                 incomplete paths and possibly overriding files.
//
//                 This routine is the main handling point for a
//                 directory request - it may return any of several
//                 things but a response is always sent.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 17, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdFileHandler::DoDirectory(HttpdRequest         *p_request, 
                                   const char           *p_file_path,
                                   const char           *p_req_path,
                                   const char           *p_decoded,
                                   const HttpdFileInfo  &info,
                                   bool                  is_get)
{
  // If the path name has no trailing slash then most browsers
  // don't get relative URI's correct when they send them back. 
  // 
  // So, if the URI does not have a trailing slash, we send back
  // a permanant redirect to the URI with a trailing slash.
  if (HttpdUtilities::HasTrailingSlash(p_request->Path()))
#if defined(HTTPD_INC_DIRECTORY_LISTS)
    ListDirectory(p_file_path, p_request, p_decoded, info, is_get);
#else
    p_request->Respond(HTTPD_RESP_NOT_FOUND);
#endif
  else
  {
    char *p_normalized = HttpdUtilities::Normalize(p_req_path, 
                                                   mpPrefix);
    char *p_new_dir = HttpdUtilities::StrVCat(p_normalized,
                                              HttpdUtilities::mRoot,
                                              (char *)0);
    HttpdOpSys::Free(p_normalized);
    if (p_new_dir != NULL)
    {
      p_request->Redirect(HTTPD_RESP_MOVED_PERM, p_new_dir);
      HttpdOpSys::Free(p_new_dir);
    }
    else
     p_request->Respond(HTTPD_RESP_SRV_ERROR);
  }
}

static const char index_html[] = "index.html";

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::SendIndexFile
//
// ACCESS:         public
//
// DESCRIPTION:    See if we have an index file to send.
//
// RETURNS:        True if there was an index file and the appropriate
//                 response was sent, false if no response was sent.
//
// REMARKS:        We determine if we have an index file, in which case
//                 we assume that a directory listing should not be allowed.
//
//                 In fact, it would be bad to let a directory listing
//                 slip out because of an error - it may be a security
//                 hazard. Therefore, if this routine encounters an
//                 error, a negative response is sent back instead of
//                 allowing the directory listing to continue.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 18, 2001
// LAST MODIFIED:  Jan 26, 2002  BY:  MYG (M1009: Add separator if needed)
//                 Jun 24, 2002       MYG (M1024: Use different redirect)
//
//--------------------------------------------------------------------------
bool HttpdFileHandler::SendIndexFile(HttpdRequest  *p_request,
                                     const char    *p_file_path, 
                                     const char    *p_req_path,
                                     bool           is_get)
{
  HttpdFileInfo  file_info;
  char          *p_index_path;
  const char    *p_separator;
  const char    *p_pfx_sep;
  bool           at_root;

  p_pfx_sep = HttpdUtilities::HasTrailingSlash(p_file_path)
            ? "" : HttpdUtilities::mRoot;
  p_index_path = HttpdUtilities::StrVCat(p_file_path, 
                                         p_pfx_sep,
                                         index_html,
                                         (char *)0);
  if (p_index_path == NULL)
  {
    p_request->Respond(HTTPD_RESP_SRV_ERROR);
    return (true);
  }
  
  int xerrno = mpFileSystem->FileInfo(p_index_path, file_info);
  HttpdOpSys::Free(p_index_path);

  if (xerrno != 0)
  {
    if (xerrno == HttpdOpSys::ERR_NOTFOUND) // Did we encounter another error?
      return (false);      // Nope, proceed with listing.
    else
      goto error;
  }
  else if (file_info.IsDir())
    return (false);

  p_separator = HttpdUtilities::mRoot;
  at_root =  (*p_req_path == '\0')
          && (HttpdUtilities::HasTrailingSlash(Prefix()));
  if (at_root || HttpdUtilities::HasTrailingSlash(p_req_path))
    p_separator++;

  // Okay, now let's send out a redirect to the real file.
  p_pfx_sep = HttpdUtilities::HasTrailingSlash(Prefix())
            ? "" : HttpdUtilities::mRoot;
  p_index_path = HttpdUtilities::StrVCat(Prefix(),
                                         p_pfx_sep,
                                         p_req_path,
                                         p_separator, 
                                         index_html,
                                         (char *)0);

  p_request->Redirect(HTTPD_RESP_MOVED_TEMP, p_index_path);
  HttpdOpSys::Free(p_index_path);
  
  return (true);

error:
  p_request->Respond(HTTPD_RESP_SRV_ERROR);
  return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdFileHandler::Handle
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
// AUTHOR:         M. Grosberg   
// CREATED:        Mar 14, 2001
// LAST MODIFIED:  Jan 22, 2002  BY:  MYG (New filesystem abstraction)
//                 Feb  5, 2002       MYG (Provisions for internal files)
//
//--------------------------------------------------------------------------
bool HttpdFileHandler::Handle(HttpdRequest *p_request)
{
  const char *p_req_path = IsMyPath(p_request);

  if (p_req_path != NULL)
  {
    // Check the method.
    bool is_get  = (strcmp(p_request->Method(), 
                    HttpdUtilities::mGetRequest) == 0);
    bool is_head = (strcmp(p_request->Method(), 
                    HttpdUtilities::mHeadRequest) == 0);

    if ((!is_get) && (!is_head))
    {
      p_request->Respond(HTTPD_RESP_METHOD_NOT_IMPL);
      return (true);
    }

    // The error handling here is rather parasitic to the interface of
    // the HttpdUtilities::Normalize function. That function can handle
    // a NULL pointer and treat it (silently) like an empty string. 
    //
    // This saves a few bytes of error handling code space at the cost
    // of maintainability.
    char  *p_decoded = HttpdUtilities::UriDecode(p_req_path);
    char  *p_file_path = HttpdUtilities::Normalize(p_decoded,
                                                   mpRootPath);

    if ((p_decoded == NULL) || (p_file_path == NULL))
      p_request->Respond(HTTPD_RESP_SRV_ERROR);
    else
    {
      HttpdFileInfo  file_info;

      // Decide what to do. 
      int xerrno = mpFileSystem->FileInfo(p_file_path, file_info);
      if (xerrno != 0)
        ErrorHandler(p_request, xerrno);
      else
      {
        if (file_info.IsDir())
        {
          if (!SendIndexFile(p_request, p_file_path, p_req_path, is_get))
            DoDirectory(p_request, 
                        p_file_path, 
                        p_req_path, 
                        p_decoded, 
                        file_info,
                        is_get);
        }
        else if (strcmp(file_info.MimeType(), 
                        "x-server-internal/private") != 0)
          SendFile(p_file_path, p_request, file_info, is_get);
        else
          ErrorHandler(p_request, HttpdOpSys::ERR_SYSPERM);
      }

      HttpdOpSys::Free(p_file_path);
      HttpdOpSys::Free(p_decoded);
    }

    return (true);
  }
  else
   return (false); // We don't handle this...
}

