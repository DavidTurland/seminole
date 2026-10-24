//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_lzarirf.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Files in ROM filesystem comrpessed with the LZARI
//                  compression algorithm.
//
//                  LZARI is an agressive coder. It requires more CPU 
//                  power but is useful for very good compression. 
//
//                  A good use for this compressor is objects that are
//                  infrequently accessed with a non-unifor distribution
//                  among bytes, such as help documents.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jun 23, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |06/23/02| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include "sem_romfs.h"


/*
 *
 * If you are not familiar with arithmetic compression, you should read:
 *
 *   I. E. Witten, R. M. Neal, and J. G. Cleary,
 *   Communications of the ACM, Vol. 30, pp. 520-540 (1987),
 *
 * from which much has been borrowed.  
 */



/*
 * Q1 (= 2 to the M) must be sufficiently large, but not so
 * large as the unsigned long 4 * Q1 * (Q1 - 1) overflows.  
 */

#define Q1      (1UL << LZARI_M)
#define Q2      (2 * Q1)
#define Q3      (3 * Q1)
#define Q4      (4 * Q1)
#define MAX_CUM (Q1 - 1)

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::GetBit
//
// ACCESS:         private
//
// DESCRIPTION:    Obtain one bit from the input stream.
//
// RETURNS:        The value of the bit.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdLzAriDecompressor::GetBit()
{
   if ((mCurMask >>= 1) == 0)
   {
     // Check to see if we ran out of bits. If we did, just return
     // 0. We don't have much of a way to fix this anyhow.
     if (mCurOffset > mLength)
        return (0);
     
     mBuffer  = mpData[mCurOffset++];
     mCurMask = 128;
   }

   return ((mBuffer & mCurMask) != 0);
}


//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::StartModel
//
// ACCESS:         private
//
// DESCRIPTION:    Initialize the statistical modeler of the 
//                 decompression engine.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdLzAriDecompressor::StartModel()
{
   int sym;
   
   mSymCum[LZARI_N_CHAR] = 0;
   for(sym = LZARI_N_CHAR; sym >= 1; sym--)
   {
      int ch           = sym - 1;

      // Default idempotent map.
      mCharToSym[ch]   = sym;
      mSymToChar[sym]  = ch;

      mSymFreq[sym]    = 1;
      mSymCum[sym - 1] = mSymCum[sym] + 1;
   }

   mSymFreq[0]           = 0;  /* sentinel (!= sym_freq[1]) */
   mPositionCum[LZARI_N] = 0;

   /* Default position distribution -- a guess. */
   for(sym = LZARI_N; sym >= 1; sym--)
      mPositionCum[sym - 1] = mPositionCum[sym] + 10000 / (sym + 200);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::UpdateModel
//
// ACCESS:         private
//
// DESCRIPTION:    Update statistics given new a new symbol.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdLzAriDecompressor::UpdateModel(int sym)
{
   int i;
   
   if (mSymCum[0] >= MAX_CUM) 
   {
      int c = 0;
      for(i = LZARI_N_CHAR; i > 0; i--) 
      {
         mSymCum[i]  = c;
         c          += (mSymFreq[i] = (mSymFreq[i] + 1) >> 1);
      }

      mSymCum[0] = c;
   }

   for(i = sym; mSymFreq[i] == mSymFreq[i - 1]; i--) 
     ;

   if (i < sym) 
   {
      int ch_i            = mSymToChar[i];
      int ch_sym          = mSymToChar[sym];

      mSymToChar[i]       = ch_sym;
      mSymToChar[sym]     = ch_i;
      mCharToSym[ch_i]    = sym;  
      mCharToSym[ch_sym]  = i;
   }

   mSymFreq[i]++;

   while (--i >= 0) 
     mSymCum[i]++;
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::BinarySearchSym
//
// ACCESS:         private
//
// DESCRIPTION:    Locate the symbol by cumulative frequency.
//
// RETURNS:        1      if x >= mSymCum[1]
//                 N_CHAR if mSymCum[N_CHAR] > x
//                 p such that mSymCum[p - 1] > x >= mSymCum[p]
//                 otherwise.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdLzAriDecompressor::BinarySearchSym(unsigned int x)
{
   int low  = 1;
   int high = LZARI_N_CHAR;

   while (low < high) 
   {
      int mid = (low + high) / 2;
      if (mSymCum[mid] > x)
        low  = mid + 1;  
      else 
        high = mid;
   }

   return (low);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::BinarySearchPos
//
// ACCESS:         private
//
// DESCRIPTION:    Locate the symbol by cumulative frequency.
//
// RETURNS:        0      if x >= mPositionCum[1]
//                 N - 1  if mPositionCum[N] > x
//                 p such that mPositionCum[p] > x >= mPositionCum[p + 1]
//                 otherwise.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdLzAriDecompressor::BinarySearchPos(unsigned int x)
{
   int low  = 1;
   int high = LZARI_N;

   while (low < high)
   {
      int mid = (low + high) / 2;
      if (mPositionCum[mid] > x)
        low  = mid + 1;
      else
        high = mid;
   }

   return (low - 1);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::StartDecode
//
// ACCESS:         private
//
// DESCRIPTION:    Prime mValue with (M + 2) bits.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdLzAriDecompressor::StartDecode()
{
   int i;

   for(i = 0; i < LZARI_M + 2; i++)
      mValue = (2 * mValue) + GetBit();
}


//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::DecodeChar
//
// ACCESS:         private
//
// DESCRIPTION:    Given a character encoding in the stream, decode it.
//
// RETURNS:        Index of the decoded symbol into the model.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdLzAriDecompressor::DecodeChar()
{
   int            sym;
   unsigned long  range;
   
   range  = mHigh - mLow;
   sym    = BinarySearchSym((unsigned int)
                            (((mValue - mLow + 1) * mSymCum[0] - 1) / range));
   mHigh  = mLow + (range * mSymCum[sym - 1]) / mSymCum[0];
   mLow  += (range * mSymCum[sym]) / mSymCum[0];

   for(;;)
   {
      if (mLow >= Q2) 
      {
         mValue -= Q2;  
         mLow   -= Q2;
         mHigh  -= Q2;
      } 
      else if ((mLow >= Q1) && (mHigh <= Q3))
      {
         mValue -= Q1;
         mLow   -= Q1;
         mHigh  -= Q1;
      }
      else if (mHigh > Q2)
         break;

      mLow   += mLow;
      mHigh  += mHigh;
      mValue  = (2 * mValue + GetBit());
   }

   int ch = mSymToChar[sym];
   UpdateModel(sym);

   return (ch);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::DecodePosition
//
// ACCESS:         private
//
// DESCRIPTION:    Given a position encoding in the stream, decode it.
//
// RETURNS:        Position index.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
int HttpdLzAriDecompressor::DecodePosition()
{
   int                position;
   unsigned long int  range;
   
   range    = mHigh - mLow;
   position = BinarySearchPos((unsigned int)
                              (((mValue - mLow + 1) 
                               * mPositionCum[0] - 1) / range));

   mHigh    = mLow 
            + (range * mPositionCum[position    ]) / mPositionCum[0];
   mLow    += (range * mPositionCum[position + 1]) / mPositionCum[0];

   for(;;)
   {
      if (mLow >= Q2)
      {
         mValue -= Q2;
         mLow   -= Q2;
         mHigh  -= Q2;
      } 
      else if ((mLow >= Q1) && (mHigh <= Q3))
      {
         mValue -= Q1;
         mLow   -= Q1;
         mHigh  -= Q1;
      }
      else if (mHigh > Q2) 
         break;

      mLow   += mLow; 
      mHigh  += mHigh;
      mValue  = (2 * mValue + GetBit());
   }

   return (position);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriDecompressor::Decode
//
// ACCESS:         public
//
// DESCRIPTION:    Decompress the stream into the output buffer.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdLzAriDecompressor::Decode(unsigned int         clength, 
                                    unsigned const char *p_data, 
                                    size_t               bufsz,
                                    unsigned char       *p_output)
{
   int                i, j, k, r, c;
   unsigned long int  count, textsize;

   mCurMask   = 0;
   mCurOffset = 0;
   mLength    = clength;
   mpData     = p_data;

   mLow       = 0;
   mHigh      = Q4;
   mValue     = 0;

   if ((textsize = bufsz) == 0)
      return;

   StartDecode();  
   StartModel();

   for(i = 0; i < LZARI_N - LZARI_F; i++) 
      mTextBuf[i] = 0;

   r = LZARI_N - LZARI_F;
   for(count = 0; count < textsize; ) 
   {
      c = DecodeChar();
      if (c < 256)  // Is it a position or character?
      {
         if (bufsz == 0)
           return;
         *p_output++ = c;
         bufsz--;

         mTextBuf[r++]  = c;
         r             &= (LZARI_N - 1);

         count++;
      }
      else 
      {
         i = (r - DecodePosition() - 1) & (LZARI_N - 1);
         j = c - 255 + LZARI_THRESHOLD;
         for(k = 0; k < j; k++) 
         {
            c = mTextBuf[(i + k) & (LZARI_N - 1)];
            if (bufsz == 0)
              return;
            *p_output++ = c;
            bufsz--;

            mTextBuf[r++]  = c;
            r             &= (LZARI_N - 1);  

            count++;
         }
      }
   }
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzAriCompressedRomFile::Decompress
//
// ACCESS:         public
//
// DESCRIPTION:    Decompress the requested chunk.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 23, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdLzAriCompressedRomFile::Decompress(const unsigned char *p_buffer)
{
   // Check to see if this chunk is not compressed. 
   //
   // This may be the case if the encoding would have resulted in a larger
   // output than a straight copy. 
   if (p_buffer[0] == 1)
   {
     memcpy(mpUncompressed, p_buffer + 1, mChunkSpace - 1);
     return;
   }
   else
     p_buffer++;

   mEngine.Decode(mChunkSpace - 1, p_buffer,
                  mChunkSize,      mpUncompressed);
}

