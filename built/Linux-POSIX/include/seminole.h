//---------------------------------------------------------------------------
//
// MODULE NAME:     seminole.h
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World 
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Declarations for the Seminole micro web server.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Mar 18, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |03/18/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |08/18/01| Create HttpdSocket::LOCALHOST const,     | AMH  |
//      |        | SHUT_RDWR/XPG4 define,                   |      |
//      |        | HTTPD_CONN_BACKLOG.                      |      |
// -----+--------+------------------------------------------+------+---------
//  003 |08/26/01| Addition of HttpdRedirector.             | AMH  |
// -----+--------+------------------------------------------+------+---------
//  004 |08/27/01| Updated prototype for ResponseBody()     | SPC  |
//      |        | for HTTP/0.9 support.                    |      |
// -----+--------+------------------------------------------+------+---------
//  005 |08/28/01| Added HttpdResponse class, and removed   | SPC  |
//      |        |     Httpd::ResponseName()                |      |
//      |        |     Httpd::ErrorPage()                   |      |
//      |        | Also added all HTTP response codes.      |      |
// -----+--------+------------------------------------------+------+---------
//  006 |09/18/01| Add continued MIME-header line support.  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  007 |09/23/01| Protocol response negotiation.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  008 |09/23/01| Add HTTP status arg to HttpdRedirector.  | AMH  |
// -----+--------+------------------------------------------+------+---------
//  009 |09/30/01| Fix response body sending decision code. | MYG  |
//      |        | Make request methods common.             |      |
// -----+--------+------------------------------------------+------+---------
//  010 |10/06/01| Added CGI support.                       | MYG  |
//      |        | Added HttpdTimeStamp class.              |      |
// -----+--------+------------------------------------------+------+---------
//  011 |10/18/01| Handle If-Modified-Since optimizations.  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  012 |10/23/01| New filesystem abstraction.              | MYG  |
//      |        | Also added feature selectability via the |      |
//      |        | C preprocessor.                          |      |
// -----+--------+------------------------------------------+------+---------
//  013 |10/27/01| Separate out target-specific classes.    | MYG  |
// -----+--------+------------------------------------------+------+---------
//  014 |10/28/01| Add hierarchical path parsing.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  015 |10/30/01| Handler removal routines.                | MYG  |
// -----+--------+------------------------------------------+------+---------
//  016 |11/06/01| Case-insensitivity fix.                  | MYG  |
// -----+--------+------------------------------------------+------+---------
//  017 |11/14/01| Remove warning from incorrect struct.    | MYG  | M1000
// -----+--------+------------------------------------------+------+---------
//  018 |01/11/02| Make ResponseMsg initializable for some  | MYG  | M1004
//      |        | C++ compilers.                           |      |
// -----+--------+------------------------------------------+------+---------
//  019 |01/14/02| Add content sink mechanism.              | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//  020 |01/18/02| Use strftime and not time_t.             | MYG  | M1007
// -----+--------+------------------------------------------+------+---------
//  021 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//  022 |02/05/02| Handle a complete URI in the request     | MYG  | M1011
// -----+--------+------------------------------------------+------+---------
//  023 |02/11/02| Add a method to determine the target     | MYG  |
//      |        | host name in the request object.         |      |
// -----+--------+------------------------------------------+------+---------
//  024 |04/14/02| Persistent connections.                  | AMH  | A1001
// -----+--------+------------------------------------------+------+---------
//  025 |05/31/02| Optimzations in sem_time.cpp.            | SPC  | S1004
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#ifndef SEMINOLE_HTTPD_H
#define SEMINOLE_HTTPD_H

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>

// A simple macro to determine the number of elements in an array.
#define HTTPD_NUMELEM(a)  (sizeof(a) / sizeof(a[0]))

#define HTTPD_INC_DIRECTORY_LISTS
#define HTTPD_INC_MODIFIED_SINCE
#define HTTPD_INC_PERSISTENT_CONN
#define HTTPD_INC_LZRW1KH_COMPRESSION
#define HTTPD_INC_LZARI_COMPRESSION

// System limits.
const size_t       HTTPD_MAX_MIME_ENTRIES         = 48;      // Pairs
const size_t       HTTPD_MAX_INPUT_LINE           = 512;     // Bytes
const size_t       HTTPD_XFER_BUF_SIZE            = 128;     // Bytes
const size_t       HTTPD_SINK_BUFFER_SIZE         = 512;     // Bytes
const unsigned int HTTPD_FIRST_TIMEOUT            = 160;     // Seconds
const unsigned int HTTPD_MIME_TIMEOUT             = 30;      // Seconds
const unsigned int HTTPD_HCLOSE_TIMEOUT           = 60 * 60; // Seconds
const size_t       HTTPD_MAX_REQUESTS_PER_CONN    = 5;       // # of reqs 
                                                             // per connection

// Current versions of the HTTP protocol.
//
// We always store the protocol as an unsigned number with
// the following forumula:
//
//     protid = (high_part * 256) + low_part;
//
typedef unsigned short HttpdProtocolVersion;

const HttpdProtocolVersion  HTTPv09 = 0x0090;
const HttpdProtocolVersion  HTTPv10 = 0x0100;
const HttpdProtocolVersion  HTTPv11 = 0x0101;
                                          
// HTTP response codes:
const int    HTTPD_RESP_CONTINUE           = 100;
const int    HTTPD_RESP_PROTOCOL           = 101;
const int    HTTPD_RESP_OK                 = 200;
const int    HTTPD_RESP_CREATED            = 201;
const int    HTTPD_RESP_ACCEPTED           = 202;
const int    HTTPD_RESP_NONAUTH_INFO       = 203;
const int    HTTPD_RESP_NO_CONTENT         = 204;
const int    HTTPD_RESP_SOME_CONTENT       = 205;
const int    HTTPD_RESP_MULTI_CHOICE       = 300;
const int    HTTPD_RESP_MOVED_PERM         = 301;
const int    HTTPD_RESP_MOVED_TEMP         = 302;
const int    HTTPD_RESP_SEE_OTHER          = 303;
const int    HTTPD_RESP_NOT_MODIFIED       = 304;
const int    HTTPD_RESP_USE_PROXY          = 305;
      //                                   = 306; // not used
const int    HTTPD_RESP_NEO_TEMP_MOVED     = 307;
const int    HTTPD_RESP_BAD_REQ            = 400;
const int    HTTPD_RESP_UNAUTHORIZED       = 401;
const int    HTTPD_RESP_PAYMENT_REQ        = 402;
const int    HTTPD_RESP_FORBIDDEN          = 403;
const int    HTTPD_RESP_NOT_FOUND          = 404;
const int    HTTPD_RESP_METHOD_NOT_ALLOWED = 405;
const int    HTTPD_RESP_NOT_ACCEPTABLE     = 406;
const int    HTTPD_RESP_PROXY_AUTH         = 407;
const int    HTTPD_RESP_REQUEST_TIMEOUT    = 408;
const int    HTTPD_RESP_CONFLICT           = 409;
const int    HTTPD_RESP_GONE               = 410;
const int    HTTPD_RESP_LENGTH_REQ         = 411;
const int    HTTPD_RESP_PRECOND_FAILED     = 412;
const int    HTTPD_RESP_TOO_LARGE          = 413;
const int    HTTPD_RESP_URI_TOO_LARGE      = 414;
const int    HTTPD_RESP_UNSUPPORTED_MEDIA  = 415;
const int    HTTPD_RESP_RANGE              = 416;
const int    HTTPD_RESP_EXPECTATION_FAILED = 417;
const int    HTTPD_RESP_SRV_ERROR          = 500;
const int    HTTPD_RESP_METHOD_NOT_IMPL    = 501;
const int    HTTPD_RESP_BAD_GATEWAY        = 502;
const int    HTTPD_RESP_UNAVAILABLE        = 503;
const int    HTTPD_RESP_GATEWAY_TIMEOUT    = 504;
const int    HTTPD_RESP_HTTP_VERSION       = 505;

// Pre-OS classes:

class HttpdWritable
{
public:
  virtual int Write(size_t n, const void *p_data)=0;
  int Printf(const char *p_format, ...);
};

// Forward references:
class Httpd;
class HttpdHandler;
class HttpdRequest;
union HttpdParameter;
class HttpdDataSource;
class HttpdFileInfo;
class HttpdFileSystem;

// Include target dependant definitions.
#include "sem_sys.h"

union HttpdParameter
{
  void            *mpVoid;
  long             mLong;
  unsigned long    mULong;
  HttpdIpAddress   mAddress; 
};

struct HttpdPair
{
  const char   *mpKey;
  const char   *mpValue;
};

struct HttpdTimeStamp
{
  unsigned int   mDay, mMonth, mYear;
  unsigned int   mHour, mMinute, mSecond;
  
  bool Parse(const char *p_str_rep);
  bool Convert(const struct tm *p_tm);
  int  Compare(const HttpdTimeStamp *p_to);

protected:
  static unsigned char DaysInMonth(unsigned int year,
                                   unsigned int month);
  bool Validate();

private:
  static const char mMonthTable[]; 
  static const char mDayTable[];
  static const unsigned char mDaysInMonth[];
  static unsigned int FindString(const char pp_table[], const unsigned int, const char *p_str);
  
  bool ParseTime(char *p_time);
  bool ParseRfc822(char *p_rest);
  bool ParseRfc850(char *p_rest);  
  bool ParseAscTime(char *p_rest);
};

struct HttpdUtilities
{
  static const char mRoot[]; 
  static const char mNetTimeFormat[];
  static const char mHeadRequest[];
  static const char mGetRequest[];

  static char       *StrVCat(const char *p_first, ...);
  static char       *StrChop(char *&p_string);
  static int         StrCmpi(const char *p_a, const char *p_b);
  static char       *GetLcExtension(char *p_file_name);
  static void        FormatTime(char       *p_buf, 
                                size_t      bufsz, 
                                const char *p_format,
                                time_t      t);
  static char       *GetComponentPath(const char *p_uri, 
                                      const char *p_filename);
  static char       *Normalize(const char *uri, 
                               const char *prefix);
  static bool        HasTrailingSlash(const char *p_path);
  static bool        HasPrefix(const char *p_str, const char *p_prefix);
  static bool        IsUriProtocol(const char *p_uri);
  static char       *UriEncode(const char *p_uri);
  static char       *UriDecode(const char *p_encoded);
  static char       *HtmlQuote(const char *p_str);

  static const char *Lookup(const HttpdPair *p_table,
                            size_t           table_size,
                            const char      *p_key,
                            int            (*p_comp)(const char *p_a,
                                                     const char *p_b)
                                           = (strcmp));

  static  bool ReadUntil(HttpdSocket &sock, 
                         char         term,
                         char       *&p_buffer,
                         size_t       bufsz);

};

/***************************************************************************/

class HttpdFileInfo
{
  struct tm        mLastModificationTime;
  struct tm        mCreationTime;
  unsigned long    mSize;
  const char      *mpMimeType;
  bool             mFreeMimeType;
  bool             mIsDir;
  bool             mFreeLocation;
  HttpdParameter   mLocation;
  HttpdFileSystem *mpFileSystem;
  
public:
  HttpdFileInfo()
  : mFreeMimeType(false), mFreeLocation(false)
  {}

  void FileSystem(HttpdFileSystem *p_fs)
  { mpFileSystem = p_fs; }

  void LastModificationTime(const struct tm *p_tm)
  { memcpy(&mLastModificationTime, p_tm, sizeof(struct tm)); }
  
  void CreationTime(const struct tm *p_tm)
  { memcpy(&mCreationTime, p_tm, sizeof(struct tm)); }
  
  void Size(unsigned long sz)
  { mSize = sz; }
  
  void IsDir(bool isit)
  { mIsDir = isit; }
  
  void MimeType(const char *p_type, bool must_free)
  {
    if (mFreeMimeType)
      free((void *)mpMimeType);
      
    mpMimeType    = p_type;
    mFreeMimeType = must_free;
  }
  
  void Location(HttpdParameter p, bool must_free)
  { mLocation = p; mFreeLocation = must_free; }

  HttpdFileSystem *FileSystem() const
  { return (mpFileSystem); }

  const struct tm *LastModificationTime() const
  { return (&mLastModificationTime); }
  
  const struct tm *CreationTime() const
  { return (&mCreationTime); }
  
  unsigned long Size() const
  { return (mSize); }
  
  bool IsDir() const
  { return (mIsDir); }
  
  const char *MimeType() const
  { return (mpMimeType); }

  HttpdParameter Location() const
  { return (mLocation); }

  ~HttpdFileInfo();
};

class HttpdFile
{
public:
  enum 
  {
    FILE_SEEK_START,
    FILE_SEEK_CUR,
    FILE_SEEK_END
  };

  virtual int Read(void *p_buffer, size_t &sz)=0;
  virtual int Write(void *p_buffer, size_t sz);
  virtual int Seek(long offset, int whence)=0;
  virtual void Close()=0;
  
  virtual int PushToSink(HttpdWritable *p_sink);
};

class HttpdDirectory
{
public:
  virtual const char *Name()  = 0;
  virtual int         Next()  = 0;
  
  virtual void        Close() = 0;
};

class HttpdFileSystem
{
protected:
  int  CommonFileInfo(HttpdFileInfo &info);

public:
  enum
  {
    FILE_READ_ONLY,
    FILE_READ_WRITE
  };

  virtual int FileInfo(const char    *p_path, 
                       HttpdFileInfo &info) = 0;
  virtual int Open(const HttpdFileInfo  &info,
                   int                   mode,
                   HttpdFile           *&p_file) = 0;
  virtual int Open(const HttpdFileInfo &info,
                   HttpdDirectory *&p_directory);
};

/***************************************************************************/

const unsigned long   HTTPD_USHRT_SZ = 2;
const unsigned long   HTTPD_ULONG_SZ = 4;

class HttpdDataSource
{
public:
    virtual int  ReadAt(void          *p_data, 
                        size_t         sz, 
                        unsigned long  offset)=0;
                        
    virtual const void *AddressOf(unsigned long offset, size_t sz);
    virtual void ReleaseAddress(const void *p_data);
    
    int  ReadAt(unsigned long    &lval, 
                unsigned long     offset);
    int  ReadAt(unsigned short   &sval, 
                unsigned long     offset);
};

class HttpdMemoryDataSource
{
    size_t                mSize;
    const unsigned char  *mpData;
    
public:
    HttpdMemoryDataSource(const void *p_data, size_t sz)
    : mSize(sz), mpData((const unsigned char *)p_data)
    {}
    
    virtual int  ReadAt(void          *p_data, 
                        size_t         sz, 
                        unsigned long  offset);
    virtual const void *AddressOf(unsigned long offset, size_t sz);
};

/***************************************************************************/

class HttpdRomFileSystem : public HttpdFileSystem
{
   HttpdDataSource         *mpDataSource;
   struct tm                mPackageTime;
   unsigned long            mMimeTable;
   unsigned long            mFileTable;
   unsigned short           mFileCount;
  
protected:
   int FindFile(const char *p_name, unsigned long &pos);
   int ParseTime();
   
public:
   int Mount(HttpdDataSource *p_source);
   
   virtual int FileInfo(const char    *p_path, 
                        HttpdFileInfo  &info);
   virtual int Open(const HttpdFileInfo  &info,
                    int                   mode,
                    HttpdFile           *&p_file);
   virtual int Open(const HttpdFileInfo  &info,
                    HttpdDirectory      *&p_directory);
};

/***************************************************************************/

class HttpdContentSink : public HttpdWritable
{
  struct SinkBuffer
  {
    SinkBuffer  *mpNext;
    char         mData[HTTPD_SINK_BUFFER_SIZE];
  };

  SinkBuffer    *mpHead;
  SinkBuffer    *mpTail;
  size_t         mTailLength;
  size_t         mContentLength;

protected:
  SinkBuffer *GetBuffer();

public:
  HttpdContentSink();
  virtual ~HttpdContentSink();

  size_t ContentLength() { return (mContentLength); }
  int    SendData(HttpdWritable *p_sink);

  virtual int Write(size_t n, const void *p_data);
};

struct HttpdResponseMsg
{
  int                    mStatus;
  const char            *mpName;
  const char            *mpDescription;
  HttpdProtocolVersion   mVersion;

  // Find appropriate response.
  static const HttpdResponseMsg &Find(int resp);
  
  // Find response appropriate for version.
  static const HttpdResponseMsg &Find(int resp, HttpdProtocolVersion vers);
};

class HttpdRequest
{
  HttpdPair            *mpHeaders;
  size_t                mHeaderCount;
  HttpdSocket           mSocket;
  char                 *mpRequest;
  char                 *mpMethod;
  char                 *mpPath;
  char                 *mpCompleteUri;
  char                 *mpRequestedHostName;
  Httpd                *mpServer;
  HttpdIpAddress        mAddress; 
  bool                  mNeedHeaders;
  bool                  mLastReq;
  HttpdProtocolVersion  mRequestVersion;

  void FreeMimeStrings();
  bool ReadMimeHeader(char *p_result);
  bool ReadContinuedMime(char *p_result);
  
public:
  HttpdRequest(HttpdParameter sock, HttpdIpAddress address, Httpd *p_server);
  ~HttpdRequest();

  const char *Header(const char *p_mime)
  {
    return (HttpdUtilities::Lookup(mpHeaders, 
                                   mHeaderCount, 
                                   p_mime,
                                   HttpdUtilities::StrCmpi));
  } 

  Httpd  *Server()
  { return (mpServer); }
 
  HttpdSocket &Socket()
  { return (mSocket);  }
  
  const char *CompleteUri()
  { return (mpCompleteUri); }
  
  const char *Method()
  { return (mpMethod); }

  HttpdProtocolVersion Protocol()
  { return (mRequestVersion); }

  char *Path()
  { return (mpPath); }

  const HttpdIpAddress ClientAddr() 
  { return (mAddress); }

  bool NeedHeaders()
  { return (mNeedHeaders); }

  bool LastReq()
  { return (mLastReq); }

  void SetLastReq()
  { mLastReq = true; }
  
  const char *RequestedHostName();

  bool ResponseBodyNeeded(int status);
  void ResponseHeader(const HttpdResponseMsg &response);
  void ResponseBody(const HttpdResponseMsg &response, const char *p_url);
  void Respond(int status);
  void Redirect(int status, 
                const char *p_location);
                
  static HttpdProtocolVersion ParseHttpVersion(const char *p_version);
};

class HttpdHandler
{
  friend class Httpd;

protected:
  HttpdHandler   *mpNext;
  const char     *mpPrefix;

  char *IsMe(HttpdRequest *p_request);
  char *IsMyPath(HttpdRequest *p_request, const char sep = '/');
  
protected:
  void  Install(HttpdHandler *&p_old_head);
        
public:
  virtual ~HttpdHandler();  

  HttpdHandler *Next()
  { return (mpNext); }
  
  const char *Prefix()
  { return (mpPrefix); }

  // Override this routine for handling support.   
  virtual bool    Handle(HttpdRequest *p_request) = 0;
};

struct HttpdCgiParameter
{
   HttpdPair          mPair;
   HttpdCgiParameter *mpNext;
   
   static void FreeList(HttpdCgiParameter *p_list);
   static HttpdCgiParameter *ParseUriString(char *p_uri);
   static HttpdCgiParameter *ParsePostData(HttpdRequest *p_request);

   static void Append(HttpdCgiParameter *p_param,
                      HttpdCgiParameter **p_list);
   
private:
   static HttpdCgiParameter *ParseUrlEncoded(HttpdRequest *p_request);
   static char *ParseParam(char               *p_param, 
                           HttpdCgiParameter **pp_list,
                           bool               &ok);
   static bool ReadUntil(HttpdSocket &sock, 
                         char         term,
                         size_t       max_len,
                         char       *&p_buffer,
                         size_t       bufsz);
};

class HttpdRedirector : public HttpdHandler
{
  char *mpNewUri;
  int   mStatusCode;

public:
  HttpdRedirector(const char *p_prefix, const char *p_newuri, int status);
  virtual ~HttpdRedirector();
  
  virtual bool Handle(HttpdRequest *p_request);
};

class HttpdFileHandler : public HttpdHandler
{
  char            *mpRootPath;
  HttpdFileSystem *mpFileSystem;

protected:
  static void   ErrorHandler(HttpdRequest *p_request, int xerrno);
  virtual void SendFile(const char          *p_path, 
                        HttpdRequest        *p_request,
                        const HttpdFileInfo &info,
                        bool                 is_get);

#if defined(HTTPD_INC_MODIFIED_SINCE)  
  static bool   NeedToSendOut(HttpdRequest        *p_request,
                              const HttpdFileInfo &info);
#endif                              

#if defined(HTTPD_INC_DIRECTORY_LISTS)
  static void   SizeToString(char *p_out, unsigned long size);                        
  virtual void ListDirectory(const char           *p_directory, 
                             HttpdRequest         *p_request,
                             const char           *p_decoded,
                             const HttpdFileInfo  &info,
                             bool                  is_get);
  virtual bool ListEntry(HttpdWritable       *p_out,
                         const char          *p_filename, 
                         const HttpdFileInfo &info);

  virtual void ListParentDir(HttpdRequest  *p_request,
                             HttpdWritable *p_out);
                             
  virtual void DirectoryBody(HttpdRequest   *p_request,
                             HttpdDirectory *p_dir, 
                             HttpdWritable  *p_out,
                             const char     *p_directory);

#endif

  virtual bool SendIndexFile(HttpdRequest  *p_request,
                             const char    *p_file_path, 
                             const char    *p_req_path,
                             bool           is_get);

  virtual void DoDirectory(HttpdRequest         *p_request, 
                           const char           *p_file_path,
                           const char           *p_req_path,
                           const char           *p_decoded,
                           const HttpdFileInfo  &info,
                           bool                  is_get);

public:
  HttpdFileHandler(HttpdFileSystem *p_filesys,
                   const char      *p_root, 
                   const char      *p_prefix);
  virtual ~HttpdFileHandler();

  virtual bool Handle(HttpdRequest *p_request);
};

class Httpd
{
  HttpdHandler         *mpHandlers;
  HttpdSocket           mListenSock;
  HttpdIpPort           mPort;
  char                 *mpServerHost;
  enum 
  { 
    Started, 
    Stopped,
    Stopping 
  }                 mState;

  bool CreateListenSocket();
  void ProcessRequest(HttpdRequest *p_req);

protected:

  // Overridable methods:
  virtual bool Allowed(HttpdIpAddress addr);
  
  // Task templates:
  static void Acceptor(HttpdParameter httpd, 
                       HttpdParameter,
                       HttpdParameter);
  static void Server(HttpdParameter httpd, 
                     HttpdParameter sock_encap,
                     HttpdParameter client_addr);
                     
public:
  Httpd(HttpdIpPort port, const char *p_srv_host);
  virtual ~Httpd();
  
  bool Start();
  void Stop();

  void Install(HttpdHandler *p_handler);
  
  HttpdHandler *Remove(const char *p_prefix);
  HttpdHandler *Remove(HttpdHandler *p_handler);
  
  // Utility methods:
  const char *ServerHost()
  { return (mpServerHost); }
  
  HttpdIpPort Port()
  { return (mPort); }
  
  // Overridable methods for I18N:
  virtual const char *ServerName();  
};


#endif

