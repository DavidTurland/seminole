//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_romfs.h
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     ROM File System Internal Interfaces
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan 22, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/22/02| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |03/23/02| Cache buffer for performance.            | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  003 |06/23/02| Add LZARI compression.                   | MYG  | M1021
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#ifndef SEM_ROMFS_H
#define SEM_ROMFS_H

class HttpdRomFile : public HttpdFile
{
protected:
   void *operator new(size_t s)
   { return (HttpdOpSys::Malloc(s)); }

   void operator delete(void *ptr)
   { HttpdOpSys::Free(ptr); }

   virtual ~HttpdRomFile();
   
public:
  virtual void Close();
};

class HttpdRomDirectory : public HttpdDirectory
{
  friend class HttpdRomFileSystem;

  char            *mpName;
  unsigned long    mOffset;
  unsigned long    mCount;
  HttpdDataSource *mpDataSource;

protected:
  HttpdRomDirectory(unsigned long    offset, 
                    unsigned long    count, 
                    HttpdDataSource *p_source)
  : mpName(NULL), mOffset(offset), mCount(count), mpDataSource(p_source)
  {}

  void *operator new(size_t s)
  { return (HttpdOpSys::Malloc(s)); }

  void operator delete(void *ptr)
  { HttpdOpSys::Free(ptr); }
  
public:
  virtual const char *Name();
  virtual int         Next();
  
  virtual void        Close();
};

/************************************************************************/

class HttpdCompressedRomFile : public HttpdRomFile
{
   friend class HttpdRomFileSystem;

   HttpdDataSource  *mpDataSource;
   unsigned long     mOffset;
   unsigned long     mContentLength;
   unsigned long     mDataLength;
   unsigned long     mCursor;

   unsigned long     mChunkOffset;     // Offset into current chunk.
   unsigned long     mNextChunk;       // DataSource address of next chunk.

protected:
   unsigned short    mChunkSize;       // Size of current chunk (uncompressed).
   unsigned short    mChunkSpace;      // Size of current chunk (compressed).
   unsigned char    *mpUncompressed;   // Pointer to chunk buffer.
   unsigned short    mBufferSize;      // Size of buffer

   HttpdCompressedRomFile(HttpdDataSource  *p_source,
                          unsigned long     data_offset,
                          unsigned long     content_length,
                          unsigned long     data_length)
   : mpDataSource(p_source),
     mOffset(data_offset),
     mContentLength(content_length),
     mDataLength(data_length),
     mCursor(0),
     mChunkOffset(0),
     mNextChunk(data_offset),
     mChunkSize(0),
     mpUncompressed(NULL)
   {}

   virtual ~HttpdCompressedRomFile();

   int NextChunk(unsigned long &data_addr);
   int DecompressChunk(unsigned long data_addr);

   // The actual decompressor.
   virtual void Decompress(const unsigned char *p_buffer)=0;

public:
   virtual int Read(void *p_buffer, size_t &sz);
   virtual int Seek(long offset, int whence);
};

/************************************************************************/

class HttpdLzrw1KhCompressedRomFile: public HttpdCompressedRomFile
{
   friend class HttpdRomFileSystem;
   
protected:
   HttpdLzrw1KhCompressedRomFile(HttpdDataSource  *p_source,
                                 unsigned long     data_offset,
                                 unsigned long     content_length,
                                 unsigned long     data_length)
   : HttpdCompressedRomFile(p_source, 
                            data_offset, 
                            content_length, 
                            data_length)
   {}

   virtual void Decompress(const unsigned char *p_buffer);
};

/************************************************************************/

#define LZARI_N          4096    /* Size of LZ ring buffer.            */
#define LZARI_F          60      /* Longest match length.              */
#define LZARI_THRESHOLD  2       /* Size of encoding to avoid bloat.   */
#define LZARI_M          15      /* Bits for statistical coder.        */

#define LZARI_N_CHAR     (256 - LZARI_THRESHOLD + LZARI_F) 

class HttpdLzAriDecompressor
{
   unsigned char    mTextBuf[LZARI_N + LZARI_F - 1];     // LZ ring buffer
   HttpdUint32      mLow, mHigh, mValue;

   int              mCharToSym[LZARI_N_CHAR];      // Model maps.
   int              mSymToChar[LZARI_N_CHAR + 1];

   unsigned int     mSymFreq[LZARI_N_CHAR + 1];    // Frequency of symbols.
   unsigned int     mSymCum[LZARI_N_CHAR + 1];     // Cumulative freq. of syms.
   unsigned int     mPositionCum[LZARI_N + 1];     // Cumulative freq. of pos.

   // Bit fetching support.
   unsigned int         mCurMask;
   unsigned int         mCurOffset;
   unsigned int         mLength;
   unsigned int         mBuffer;
   unsigned const char *mpData;

   int  GetBit();

   // Arithmatic coder model support.
   void StartModel();
   void UpdateModel(int sym);
   int  BinarySearchSym(unsigned int x);
   int  BinarySearchPos(unsigned int x);

   // Stream parser:
   int  DecodeChar();
   int  DecodePosition();
   void StartDecode();

public:
   void Decode(unsigned int         clength, 
               unsigned const char *p_data, 
               size_t               bufsz,
               unsigned char       *p_output);
};

class HttpdLzAriCompressedRomFile : public HttpdCompressedRomFile
{
   friend class HttpdRomFileSystem;

   HttpdLzAriDecompressor    mEngine;

protected:
   HttpdLzAriCompressedRomFile(HttpdDataSource   *p_source,
                               unsigned long      data_offset,
                               unsigned long      content_length,
                               unsigned long      data_length)
   : HttpdCompressedRomFile(p_source,
                            data_offset,
                            content_length,
                            data_length)
   {}

   virtual void Decompress(const unsigned char *p_buffer);
};

/************************************************************************/

class HttpdRawRomFile : public HttpdRomFile
{
  friend class HttpdRomFileSystem;

  HttpdDataSource  *mpDataSource;
  unsigned long     mOffset;
  unsigned long     mLength;
  unsigned long     mCursor;

protected:
  HttpdRawRomFile(HttpdDataSource *p_source, 
                  unsigned long    offs, 
                  unsigned long    len);

public:
  virtual int Read(void *p_buffer, size_t &sz);
  virtual int Seek(long offset, int whence);
  virtual int PushToSink(HttpdWritable *p_sink);
};

#endif
