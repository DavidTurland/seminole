/*---------------------------------------------------------------------------
**
** MODULE NAME:     lzrw1kh_compress.c
**
** COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
**                  file within the Seminole distribution for terms of use.
**                  If your distribution is incomplete, the Seminole
**                  documentation and license are available on the World 
**                  Wide Web at http:**www.conman.org/software/seminole/
**
** DESCRIPTION:     Compression utility for SCPG using the LZRW1/KH 
**                  algorithm by Kurt Haenen.
**
** AUTHOR:          M. Grosberg
**
** CREATION DATE:   Jan 30, 2002
**
**---------------------------------------------------------------------------
**
** HISTORY OF CHANGE  
**
** -----+--------+------------------------------------------+------+---------
** VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
** =====+========+==========================================+======+=========
**  001 |01/30/02| File creation.                           | MYG  |  
** -----+--------+------------------------------------------+------+---------
**  002 |06/23/02| Remove common code to support multiple   | MYG  | M1021
**      |        | compressors.                             |      |
** -----+--------+------------------------------------------+------+---------
**-------------------------------------------------------------------------*/

#include "compress-util.h"


#define FLAG_COMPRESSED    0x40
#define FLAG_COPIED        0x80
#define RLE_THRESHOLD      16

/*---------------------------------------------------------------------------
**
** NAME:           GetMatch
**
** DESCRIPTION:    Find a match in the ring buffer.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
int  GetMatch(unsigned char *p_source,
              LzUWord        offset,
              LzUWord        source_size,
              LzWord        *p_hash,
              LzUWord       *p_size,
              LzWord        *p_pos)
{ 
   unsigned char inner_mix = ((p_source[offset] << 4) ^ p_source[offset + 1]);
   unsigned char outer_mix = ((inner_mix << 4) ^ p_source[offset + 2]);
   LzUWord  hash_value     = ((40543L * outer_mix) >> 4) & 0xfff;

   *p_pos = p_hash[hash_value];

   p_hash[hash_value] = offset;

   if (((*p_pos) != -1) && ((offset - (*p_pos)) < 4096))
   {
     for((*p_size) = 0; 
         ((*p_size < 18)
      &&  (p_source[offset + (*p_size)] == p_source[(*p_pos) + (*p_size)])
      &&  ((offset + (*p_size)) < source_size)); 
         (*p_size)++)
       ;

     return ((*p_size) >= 3);
   }
   
   return (0);
}

/*---------------------------------------------------------------------------
**
** NAME:           Compression
**
** DESCRIPTION:    This is the LZRW1KH compression engine.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
static LzUWord  Compression(unsigned char *p_source,
                            unsigned char *p_dest,
                            LzUWord        source_size)
{
  LzWord         hash[4096], pos;
  LzUWord        key, size;
    
  unsigned char  bit         = 0;
  LzUWord        command     = 0;
  LzUWord        inpos       = 0;
  LzUWord        outpos      = 3;
  LzUWord        cmdpos      = 1;

  for(key = 0; key < 4096; key++)
     hash[key] = -1;

  p_dest[0] = FLAG_COMPRESSED;
    
  while ((inpos < source_size) && (outpos <= source_size))
  {  
    if (bit > 15)
    {
      p_dest[cmdpos++]  = (command >> 8) & 0x00ff;
      p_dest[cmdpos]    = command & 0x00ff;
      cmdpos            = outpos;
      bit               = 0;
      outpos           += 2;
    }
        
    /* Look for an RLE pattern. */
    for(size = 1; 
        (p_source[inpos] == p_source[inpos + size]) 
     && (size < 0x0fff)
     && ((inpos + size) < source_size); 
        size++)
         ;

    if (size >= RLE_THRESHOLD) /* Acceptable RLE? */
    {
      p_dest[outpos++]  = 0;
      p_dest[outpos++]  = ((size - RLE_THRESHOLD) >> 8) & 0x00ff;
      p_dest[outpos++]  = (size - RLE_THRESHOLD) & 0x00ff;
      p_dest[outpos++]  = p_source[inpos];
      inpos            += size;
      command           = (command << 1) + 1;
    }
    else if (GetMatch(p_source, inpos, source_size, hash, &size, &pos))
    {
      /*
       * A string we saw before. Encode positionally.
       */
      key               = ((inpos - pos) << 4) + (size - 3);
      p_dest[outpos++]  = (key >> 8) & 0x00ff;
      p_dest[outpos++]  = key & 0x00ff;
      inpos            += size;
      command           = (command << 1) + 1;
    }
    else
    {
      /*
       * A regular old byte. Encode it normally.
       */
      p_dest[outpos++]  = p_source[inpos++];
      command           = (command << 1);
    }
       
    bit++;
  }

  /* Flush the last command word. */
  command <<= (16 - bit);

  p_dest[cmdpos++] = (command >> 8) & 0x00ff;
  p_dest[cmdpos]   = command & 0x00ff;
  if (outpos > source_size)
  {    
    /*
     * If we got here we are very unlucky. It means that the compressed
     * size is bigger than the uncompressed size. 
     *
     * So just mark the output as a copy chunk and proceed with 
     * the compression.
     */
	    memcpy(p_dest + 1, p_source, source_size);
    p_dest[0] = FLAG_COPIED;
    return (source_size + 1);
  }
     
  return (outpos);
}


