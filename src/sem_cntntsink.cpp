//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_cntntsink.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     HttpdContentSink implementation.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan  14, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/14/02| File creation.                           | MYG  | A1001
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

//---------------------------------------------------------------------------
//
// NAME:           HttpdContentSink::HttpdContentSink
//
// ACCESS:         public
//
// DESCRIPTION:    Initialize the content sink.
//
// REMARKS:        Just setup the initial pointers, don't pre-allocate
//                 any buffers.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 14, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdContentSink::HttpdContentSink()
  : mpHead(NULL), 
    mTailLength(HTTPD_SINK_BUFFER_SIZE),
    mContentLength(0)
{}

//---------------------------------------------------------------------------
//
// NAME:           HttpdContentSink::~HttpdContentSink
//
// ACCESS:         public
//
// DESCRIPTION:    Destroy the content sink.
//
// REMARKS:        Return the allocated set of buffers to the operating
//                 system.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 14, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdContentSink::~HttpdContentSink()
{
  SinkBuffer *p_walk = mpHead;

  while (p_walk != NULL)
  {
    SinkBuffer *p_next = p_walk->mpNext;

    HttpdOpSys::Free(p_walk);
    p_walk = p_next;
  }  
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdContentSink::GetBuffer
//
// ACCESS:         protected
//
// DESCRIPTION:    Allocate a new buffer
//
// RETURNS:        A pointer to the new buffer on success, NULL on
//                 failure.
//
// REMARKS:        Do not assume the contents of the buffer are 
//                 initialized (except for the link field). They are
//                 filled in debug mode but this is not a normal 
//                 configuration.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 14, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdContentSink::SinkBuffer *HttpdContentSink::GetBuffer()
{
  SinkBuffer *p_result = 
     (SinkBuffer *)HttpdOpSys::Malloc(sizeof(SinkBuffer));

  if (p_result == NULL)
    return (NULL);

  p_result->mpNext = NULL;

  if (mpHead == NULL)
    mpHead = mpTail = p_result;
  else
  {
    mpTail->mpNext  = p_result;
    mpTail          = p_result;
  }

#ifndef NDEBUG
  memset(p_result->mData, 'X', HTTPD_SINK_BUFFER_SIZE);
#endif

  return (p_result);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdContentSink::Write
//
// ACCESS:         public
//
// DESCRIPTION:    Store content into the sink, allocating buffers as
//                 necessary.
//
// RETURNS:        OS Error code
//
// REMARKS:        The member variable mTailLength is always
//                 updated to reflect the number of bytes used
//                 in the last buffer. This makes the common
//                 case of Printf() and small Write() calls
//                 more efficient.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 14, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdContentSink::Write(size_t n, const void *p_vdata)
{
  const char *p_data = (const char *)p_vdata;

  size_t remaining_length = HTTPD_SINK_BUFFER_SIZE - mTailLength;

  // Step 1: Copy into remaining length of last buffer (if any)
  if (n < remaining_length) // Data fits in all the way.
  {
    memcpy(mpTail->mData + mTailLength, p_data, n);
    mTailLength    += n;
    mContentLength += n;

    return (0);
  }
  else if (remaining_length != 0) // Fragment there?
  {
    memcpy(mpTail->mData + mTailLength, p_data, remaining_length);
    p_data         += remaining_length;
    n              -= remaining_length;
    mContentLength += remaining_length;
  }
  
  mTailLength = HTTPD_SINK_BUFFER_SIZE;

  // Step 2: Start appending buffers (if necessary)
  while (n != 0)
  {
    size_t      chunk_len;
    SinkBuffer *p_buffer;

    if (n > HTTPD_SINK_BUFFER_SIZE)
      chunk_len = HTTPD_SINK_BUFFER_SIZE;
    else
    {
      chunk_len   = 
      mTailLength = n;
    }
    
    p_buffer = GetBuffer();
    if (p_buffer == NULL)
      return (HttpdOpSys::ERR_OUTOFMEM);

    memcpy(p_buffer->mData, p_data, chunk_len);
    n              -= chunk_len;
    p_data         += chunk_len;
    mContentLength += chunk_len;
  }

  // All done.
  return (0);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdContentSink::SendData
//
// ACCESS:         public
//
// DESCRIPTION:    Send the data from the content sink into another
//                 sink (typically a socket).
//
// RETURNS:        OS Error code
//
// REMARKS:        The buffers are not freed during this operation
//                 so it may be performed at multiple times. In
//                 addition, the sink may be appended between subsequent
//                 SendData() operations. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 14, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
int HttpdContentSink::SendData(HttpdWritable *p_out)
{
  SinkBuffer  *p_walk;
  int          rv;

  for(p_walk = mpHead; p_walk != NULL; p_walk = p_walk->mpNext)
  {
    size_t  bs = (p_walk->mpNext != NULL) 
               ? HTTPD_SINK_BUFFER_SIZE
               : mTailLength;

    rv = p_out->Write(bs, p_walk->mData);
    if (rv != 0)
      return (rv);
  }

  return (0);
}
