//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_lzrw1rf.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Files in ROM filesystem comrpessed with the LZRW1/KH
//                  compression algorithm originally coded by Kurt HAENEN.
//
//                  The code in this file is derived from his LZRW1/KH 
//                  package that is available on the net.
//
//                  LZRW1 is a fast algorithm that provides quite good
//                  compression on text like HTML and XML. This is a good
//                  choice to use for infrequently accessed files.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Jan 29, 2002
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |01/29/02| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include "sem_romfs.h"

const unsigned char FLAG_COMPRESSED  = 0x40;
const unsigned char FLAG_COPIED      = 0x80;
const HttpdUint16   RLE_THRESHOLD    = 16;

//---------------------------------------------------------------------------
//
// NAME:           HttpdLzrw1KhCompressedRomFile::Decompress
//
// ACCESS:         protected
//
// DESCRIPTION:    Quick decompression of LZRW1/KH.
//
// REMARKS:        See the associated compressor in the SCPG tool.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 29, 2002
// LAST MODIFIED:  N/A           BY: N/A
//
//--------------------------------------------------------------------------
void HttpdLzrw1KhCompressedRomFile::Decompress(const unsigned char *p_buffer)
{ 
  HttpdUint16    inpos           = 3; 
  HttpdUint16    outpos          = 0; // Output offset.
  HttpdUint16    command         = (p_buffer[1] << 8) + p_buffer[2];
  unsigned char  bit             = 16;

  // Check to see if the compression phase decided that this chunk
  // be directly copied instead of compressed. It is possible
  // (but rare) that the compressed data + metadata exceeds the length
  // of the uncompressed data. 
  //
  // Of course, we hope we have better luck with other chunks.
  if (p_buffer[0] == FLAG_COPIED) 
  {
    memcpy(mpUncompressed, p_buffer + 1, mChunkSpace - 1);
    return; 
  }

  while (inpos < mChunkSpace)
  { 
    if (bit == 0) // Refill command bucket.
    {
      command  = (p_buffer[inpos++] << 8);
      command += p_buffer[inpos++];
      bit      = 16;
    }
      
    if (command & 0x8000) // Special processing?
    {
      HttpdUint16 pos;

      pos  = (p_buffer[inpos++] << 4);             
      pos += (p_buffer[inpos]   >> 4);
         
      if (pos != 0) // Positional replacement.
      {
         HttpdUint16 size = (p_buffer[inpos++] & 0x0f) + 3;
         HttpdUint16 i;

         for(i = 0; i < size; i++)
             mpUncompressed[outpos + i] = mpUncompressed[outpos - pos + i];
         outpos += size;
      }
      else // Run-length encoded.
      {
         HttpdUint16 size;

         size  = (p_buffer[inpos++] << 8);
         size += p_buffer[inpos++] 
               + RLE_THRESHOLD;

         HttpdUint16 i;

         for(i = 0; i < size; i++)
           mpUncompressed[outpos + i] = p_buffer[inpos];

         inpos++;
         outpos += size;
      }
    }
    else 
      mpUncompressed[outpos++] = p_buffer[inpos++];
     
    command <<= 1;
    bit--;
  }
}
