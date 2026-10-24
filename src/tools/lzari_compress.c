/*---------------------------------------------------------------------------
**
** MODULE NAME:     lzari_compress.c
**
** COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
**                  file within the Seminole distribution for terms of use.
**                  If your distribution is incomplete, the Seminole
**                  documentation and license are available on the World 
**                  Wide Web at http:**www.conman.org/software/seminole/
**
** DESCRIPTION:     Compression utility for SCPG using the LZARI
**                  algorithm by as published in the ACM.
**
** AUTHOR:          M. Grosberg
**
** CREATION DATE:   Jun 23, 2002
**
**---------------------------------------------------------------------------
**
** HISTORY OF CHANGE  
**
** -----+--------+------------------------------------------+------+---------
** VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
** =====+========+==========================================+======+=========
**  001 |06/23/02| File creation.                           | MYG  |  
** -----+--------+------------------------------------------+------+---------
**  002 |06/29/02| Fix wrong size return value.             | MYG  | M1025
** -----+--------+------------------------------------------+------+---------
**-------------------------------------------------------------------------*/

#include "compress-util.h"

LzULong textsize;
unsigned long codesize;

static unsigned int     put_buffer, put_mask;
static unsigned char   *output_buf;
static size_t           output_buf_size;
static unsigned char   *input_buf;
static LzUWord          input_buf_size;
static unsigned char   *output_buf_head;
static jmp_buf          abort_compression;
static LzUWord          bytes_written;

/*---------------------------------------------------------------------------
**
** NAME:           PutBit
**
** DESCRIPTION:    Write a single bit to the ouput buffer.
**
** REMARKS:        If the output grows beyond the size of the input
**                 we abort the compression, mark the chunk as 
**                 non-compressed and longjmp() out of the compression
**                 routine.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
static void PutBit(int bit)
{   
   if (bit) 
     put_buffer |= put_mask;
     
   if ((put_mask >>= 1) == 0)
   {
     if (output_buf_size > 0)
     {
       *output_buf++ = (unsigned char )(put_buffer & 0xff);
       output_buf_size--;       
       bytes_written++;
     }
     else
     {
       /* Output buffer overflow. Abort compression. */
       *output_buf_head++ = 1; 
       memcpy(output_buf_head, input_buf, input_buf_size);
       longjmp(abort_compression, 1);
     }
     
     put_buffer = 0; 
     put_mask   = 128;

     codesize++;
   }
}

/*---------------------------------------------------------------------------
**
** NAME:           FlushBitBuffer
**
** DESCRIPTION:    Write out queued up bits.
**
** REMARKS:        If any partial bits remain in the FIFO, write
**                 them out.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void FlushBitBuffer(void)
{
   int  i;
   
   for(i = 0; i < (CHAR_BIT - 1); i++) 
      PutBit(0);
}

/* Size of LZSS ring buffer. */
#define N           4096
/* Maximum size of match. */
#define F             60
/* Threshold for deciding how to encode. */
#define THRESHOLD      2
/* Root of binary search trees: */
#define NIL            N 

/* Ring buffer: */
unsigned char  text_buf[N + F - 1]; 
/* Variables updated by InsertNode: */
int      match_position, match_length;
/* Binary search trees: */
int      lson[N + 1], rson[N + 257], dad[N + 1]; 

/*---------------------------------------------------------------------------
**
** NAME:           InitTree
**
** DESCRIPTION:    Initialize the binary trees.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void InitTree(void)
{
   int  i;

   /*
    * For i = 0 to N - 1, rson[i] and lson[i] will be the right and
    * left children of node i.  These nodes need not be initialized.
    * Also, dad[i] is the parent of node i.  
    *
    * These are initialized to NIL (= N), 
    * which stands for 'not used.'
    *
    * For i = 0 to 255, rson[N + i + 1] is the root of the tree
    * for strings that begin with character i.  These are initialized
    * to NIL.  Note there are 256 trees. 
    */

   for(i = N + 1; i <= N + 256; i++) 
      rson[i] = NIL;   /* root */
   for(i = 0; i < N; i++)
      dad[i] = NIL;    /* node */
}

/*---------------------------------------------------------------------------
**
** NAME:           InsertNode
**
** DESCRIPTION:    Inserts a string.
**
** REMARKS:        Given a string of length F located at 
**                 text_buf[r..r+F-1], insert the string into the
**                 tree for the first character of the string
**                 (this is the "SS" part of the LZ). 
**
**                 The longest-match position is also updated. 
**                 If the match length is equal to the length of
**                 the string then the old node is removed because
**                 it will be deleted soon anyhow.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void InsertNode(int r)
{
   int  i, p, cmp, temp;
   unsigned char  *key;

   cmp          = 1;  
   key          = &text_buf[r];  
   p            = N + 1 + key[0];
   rson[r]      = lson[r] 
                = NIL;  
   match_length = 0;
   
   for(;;) 
   {
      if (cmp >= 0)
      {
         if (rson[p] != NIL)
            p = rson[p];
         else
         {
            rson[p] = r;  
            dad[r]  = p; 
            return;  
         }
      } 
      else
      {
         if (lson[p] != NIL)
             p = lson[p];
         else 
         {
            lson[p] = r;
            dad[r]  = p;

            return;
         }
      }

      for(i = 1; i < F; i++)
         if ((cmp = key[i] - text_buf[p + i]) != 0)  
             break;

      if (i > THRESHOLD)
      {
         if (i > match_length) 
         {
            match_position = (r - p) & (N - 1);
            if ((match_length = i) >= F) 
                break;
         } 
         else if (i == match_length)
         {
            if ((temp = (r - p) & (N - 1)) < match_position)
               match_position = temp;
         }
      }
   }

   dad[r]       = dad[p]; 
   lson[r]      = lson[p];
   rson[r]      = rson[p];
   dad[lson[p]] = r;
   dad[rson[p]] = r;
   
   if (rson[dad[p]] == p)
      rson[dad[p]] = r;
   else 
      lson[dad[p]] = r;

   dad[p] = NIL;  /* remove p */
}

/*---------------------------------------------------------------------------
**
** NAME:           DeleteNode
**
** DESCRIPTION:    Delete a node from the tree.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void DeleteNode(int p)  /* Delete node p from tree */
{
   int  q;
   
   if (dad[p] == NIL)
       return;  /* not in tree */
   if (rson[p] == NIL)
       q = lson[p];
   else if (lson[p] == NIL)
       q = rson[p];
   else
   {
      q = lson[p];
      if (rson[q] != NIL) 
      {
         do
           q = rson[q];  
         while (rson[q] != NIL);

         rson[dad[q]] = lson[q];
         dad[lson[q]] = dad[q];
         lson[q]      = lson[p]; 
         dad[lson[p]] = q;
      }
      rson[q]      = rson[p];
      dad[rson[p]] = q;
   }
   dad[q] = dad[p];
   if (rson[dad[p]] == p)
      rson[dad[p]] = q;
   else
      lson[dad[p]] = q;

   dad[p] = NIL;
}

/*
 * If you are not familiar with arithmetic compression, you should read:
 *   I. E. Witten, R. M. Neal, and J. G. Cleary,
 *   Communications of the ACM, Vol. 30, pp. 520-540 (1987),
 *
 * from which much have been borrowed.  
 */

#define M   15

/*
 * Q1 (= 2 to the M) must be sufficiently large, but not so
 * large as the unsigned long 4 * Q1 * (Q1 - 1) overflows.  
 */

#define Q1  (1UL << M)
#define Q2  (2 * Q1)
#define Q3  (3 * Q1)
#define Q4  (4 * Q1)
#define MAX_CUM (Q1 - 1)

/* character code = 0, 1, ..., N_CHAR - 1 */
#define N_CHAR  (256 - THRESHOLD + F)

unsigned long int  low, high, value;
/* counts for magnifying low and high around Q2 */
int                shifts;  
/* Symbol <-> Character mapping functions: */
int                char_to_sym[N_CHAR], sym_to_char[N_CHAR + 1];

unsigned int       sym_freq[N_CHAR + 1];  /* frequency for symbols */
unsigned int       sym_cum[N_CHAR + 1];   /* cumulative freq for symbols */
unsigned int       position_cum[N + 1];   /* cumulative freq for positions */

/*---------------------------------------------------------------------------
**
** NAME:           StartModel
**
** DESCRIPTION:    Initialize the statistical portion of the arithmatic
**                 coder.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void StartModel(void)
{
   int ch, sym, i;
   
   sym_cum[N_CHAR] = 0;
   for(sym = N_CHAR; sym >= 1; sym--)
   {
      ch               = sym - 1;
      char_to_sym[ch]  = sym;
      sym_to_char[sym] = ch;
      sym_freq[sym]    = 1;
      sym_cum[sym - 1] = sym_cum[sym] + sym_freq[sym];
   }

   sym_freq[0]     = 0;  /* sentinel (!= sym_freq[1]) */
   position_cum[N] = 0;

   /*
    * Empirical distribution function (quite tentative)
    * Perhaps we can use a better mechanism! 
    */
   for(i = N; i >= 1; i--)
      position_cum[i - 1] = position_cum[i] + 10000 / (i + 200);
}

/*---------------------------------------------------------------------------
**
** NAME:           UpdateModel
**
** DESCRIPTION:    Update the model with a new symbol.
**                 coder.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void UpdateModel(int sym)
{
   int i, c, ch_i, ch_sym;
   
   if (sym_cum[0] >= MAX_CUM) 
   {
      c = 0;
      for(i = N_CHAR; i > 0; i--) 
      {
         sym_cum[i] = c;
         c += (sym_freq[i] = (sym_freq[i] + 1) >> 1);
      }
      sym_cum[0] = c;
   }
   
   for(i = sym; sym_freq[i] == sym_freq[i - 1]; i--) 
     ;

   if (i < sym) 
   {
      ch_i                = sym_to_char[i];
      ch_sym              = sym_to_char[sym];
      sym_to_char[i]      = ch_sym;
      sym_to_char[sym]    = ch_i;
      char_to_sym[ch_i]   = sym;  
      char_to_sym[ch_sym] = i;
   }
   
   sym_freq[i]++;
   
   while (--i >= 0) 
     sym_cum[i]++;
}

/*---------------------------------------------------------------------------
**
** NAME:           Output
**
** DESCRIPTION:    Output one bit followed by its complements.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
static void Output(int bit)
{
   PutBit(bit);
   for(; shifts > 0; shifts--) 
      PutBit(!bit);
}

/*---------------------------------------------------------------------------
**
** NAME:           EncodeChar
**
** DESCRIPTION:    Output an encoded character 
**
** AUTHOR:         M. Grosberg
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void EncodeChar(int ch)
{
   int                sym;
   unsigned long int  range;

   sym    = char_to_sym[ch];
   range  = high - low;
   high   = low + (range * sym_cum[sym - 1]) / sym_cum[0];
   low   +=       (range * sym_cum[sym    ]) / sym_cum[0];
   for(;;)
   {
      if (high <= Q2)
         Output(0);
      else if (low >= Q2) 
      {
        Output(1);
        low  -= Q2; 
        high -= Q2;
      } 
      else if ((low >= Q1) && (high <= Q3))
      {
         shifts++;
         low  -= Q1;
         high -= Q1;
      } 
      else
        break;

      low  += low;
      high += high;
   }

   UpdateModel(sym);
}

/*---------------------------------------------------------------------------
**
** NAME:           EncodePosition
**
** DESCRIPTION:    Output an encoded LZSS position.
**
** AUTHOR:         M. Grosberg
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void EncodePosition(int position)
{
   unsigned long int  range;

   range  = high - low;
   high   = low + (range * position_cum[position    ]) / position_cum[0];
   low   +=       (range * position_cum[position + 1]) / position_cum[0];
   for(;;)
   {
      if (high <= Q2)
         Output(0);
      else if (low >= Q2)
      {
         Output(1);
         low  -= Q2;
         high -= Q2;
      } 
      else if (low >= Q1 && high <= Q3) 
      {
         shifts++;

         low  -= Q1;
         high -= Q1;
      }
      else
        break;

      low  += low;
      high += high;
   }
}

/*---------------------------------------------------------------------------
**
** NAME:           EncodeEnd
**
** DESCRIPTION:    Complete the encoding stream.
**
** AUTHOR:         M. Grosberg
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
void EncodeEnd(void)
{
   shifts++;

   Output(low >= Q1);
   FlushBitBuffer();
}

/*---------------------------------------------------------------------------
**
** NAME:           Compression
**
** DESCRIPTION:    LZARI compress the stream.
**
** AUTHOR:         M. Grosberg
** CREATED:        Jun 23, 2002  X 
** LAST MODIFIED:  Jun 29, 2002  BY:  MYG (M1025: Correct return value)
**
**------------------------------------------------------------------------*/
LzUWord Compression(unsigned char *p_src_buf, 
                    unsigned char *p_dst_buf, 
                    LzUWord        chunk_size)
{
   int            i;
   int            c;
   int            len;
   int            r;
   int            s;
   int            last_match_length;
   unsigned char *p_end_buf;

   /* Initiate the buffers. If output buffering overflows, we
    * force the flag at the beginning to mark the block as copied.
    */
   input_buf       = p_src_buf;
   input_buf_size  = chunk_size;
   output_buf_head = p_dst_buf;
   output_buf      = p_dst_buf + 1;
   output_buf_size = chunk_size + EXTRA_BLOCK_SIZE - 1;
   *p_dst_buf      = 0;
   bytes_written   = 0;
   p_end_buf       = p_src_buf + chunk_size;

   if (setjmp(abort_compression) == 0)
   {
     textsize   = 
     codesize   = 
     put_buffer = 0;
     put_mask   = 128;

     low    = 0;
     high   = Q4;
     value  = 0;
     shifts = 0;  
   
     textsize  = chunk_size;
     codesize += sizeof(textsize); 

     if (textsize == 0)
        return (0);
     textsize = 0;

     StartModel();  
     InitTree();

     s = 0;
     r = N - F;

     for(i = s; i < r; i++) 
        text_buf[i] = 0;
     for(len = 0; (len < F) && (p_src_buf < p_end_buf); len++)
        text_buf[r + len] = *p_src_buf++;
     textsize = len;

     for(i = 1; i <= F; i++)
        InsertNode(r - i);
   
     InsertNode(r);

     do
     {
       if (match_length > len) 
          match_length = len;
       if (match_length <= THRESHOLD)
       {
          match_length = 1; 
          EncodeChar(text_buf[r]);
       } 
       else 
       {
          EncodeChar(255 - THRESHOLD + match_length);
          EncodePosition(match_position - 1);
       }
     
       last_match_length = match_length;
       for(i = 0; 
           (i < last_match_length) && (p_src_buf < p_end_buf);
           i++)
       {
          c = *p_src_buf++;
        
          DeleteNode(s);
          text_buf[s] = c;
          if (s < F - 1)
            text_buf[s + N] = c;
        
          s = (s + 1) & (N - 1);
          r = (r + 1) & (N - 1);
          InsertNode(r);
       }

       textsize += i;

       while (i++ < last_match_length) 
       {
          DeleteNode(s);
          s = (s + 1) & (N - 1);
          r = (r + 1) & (N - 1);
          if (--len)
            InsertNode(r);
       }
    } while (len > 0);
    EncodeEnd(); 
    return (bytes_written + 1);
  }
  else
    return (input_buf_size + 1);
}

