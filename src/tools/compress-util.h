/*---------------------------------------------------------------------------
**
** MODULE NAME:     compress-util.h
**
** COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
**                  file within the Seminole distribution for terms of use.
**                  If your distribution is incomplete, the Seminole
**                  documentation and license are available on the World
**                  Wide Web at http://www.conman.org/software/seminole/
**
** DESCRIPTION:     Framework for SCPG compressors.
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
**  002 |07/10/02| Fix overflow bug.                        | MYG  | M1029
**      |        | Remove unecessary mode support.          |      | M1027
** -----+--------+------------------------------------------+------+---------
**-------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <setjmp.h>

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#ifndef EXTRA_BLOCK_SIZE
# define EXTRA_BLOCK_SIZE 16
#endif

/*
 * These definitions need to match up with what makes sense on the
 * target architecture for 16 & 32-bit words.
 */
typedef unsigned short LzUWord;
typedef short          LzWord;
typedef unsigned long  LzULong;

static FILE           *fp_in, *fp_out;
static LzUWord         chunk_size;
static unsigned char  *p_src_buf, *p_dst_buf;
static unsigned long   input_total, output_total;
static char           *argv0;
static unsigned int    short_size;

static LzUWord  Compression(unsigned char *p_source,
                            unsigned char *p_dest,
                            LzUWord        source_size);

/*---------------------------------------------------------------------------
**
** NAME:           WriteShort
**
** DESCRIPTION:    Write an HttpdUShort to the output stream.
**
** REMARKS:        This attempts to put X in the targets format.
**                 If that can not be done an error is generated.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  Jul 10, 2002  BY:  MYG (M1029: Overflow in shift)
**
**------------------------------------------------------------------------*/
static void WriteShort(unsigned short x)
{
  unsigned int i;
  unsigned int real_byte_length = sizeof(LzUWord);
  unsigned char byte;

  if (real_byte_length > short_size)
    real_byte_length = short_size;
    
  for(i = 0; i < real_byte_length; i++)
  {
    unsigned int bits = (x >> (i * CHAR_BIT));
    byte = (bits & UCHAR_MAX);
    
    if (fwrite(&byte, 1, 1, fp_out) != 1)
    {
      fprintf(stderr, 
              "%s: Can't write numerical value to output file (%s).\n", 
              argv0,
              strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  
  /* Write filler bytes (if necessary) */
  for(byte = 0; i < short_size; i++)
  {
    if (fwrite(&byte, 1, 1, fp_out) != 1)
    {
      fprintf(stderr,
              "%s: Can't write filler to output file (%s).\n",
              argv0,
              strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  
  output_total += short_size;
}

/*---------------------------------------------------------------------------
**
** NAME:           CompressChunks
**
** DESCRIPTION:    Chunk-up the input stream and compress each chunk 
**                 and write it to the output stream.
**
** REMARKS:        Enough memory must be available to hold the chunk
**                 plus the output buffer and all state overhead.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
static void CompressChunks(void)
{
  size_t   chunk_read;
  LzUWord  output_size;
  int      read_errno;
  
  for(;;)
  {
    chunk_read = fread(p_src_buf, 1, chunk_size, fp_in);
    read_errno = errno;
    if (chunk_read == 0)
      break;
    input_total += chunk_read;
      
    output_size = Compression(p_src_buf, p_dst_buf, (LzUWord )chunk_read);
    
    WriteShort(output_size);
    WriteShort((unsigned short )chunk_read);
    
    if (fwrite(p_dst_buf, 1, output_size, fp_out) != output_size)
    {
      fprintf(stderr, 
              "%s: Can't write to output file (%s).\n", 
              argv0,
              strerror(errno));
              
      exit(EXIT_FAILURE);
    }
    output_total += output_size;
    
    if (chunk_read != chunk_size)
      break;
  }

  if (!feof(fp_in))
  {
    fprintf(stderr, 
            "%s: Error reading input file (%s)\n", 
            argv0, 
            strerror(read_errno));
    
    exit(EXIT_FAILURE);
  }
}

/*---------------------------------------------------------------------------
**
** NAME:           CleanupRoutine
**
** DESCRIPTION:    Called by the C runtime when the process terminates
**                 to release any resources to the host OS.
**
** REMARKS:        We probably should assume the host OS is smart enough
**                 to do this on process destruction -- but to be safe
**                 we try and do it anyhow.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  N/A           BY:  N/A
**
**------------------------------------------------------------------------*/
static void CleanupRoutine(void)
{
  fclose(fp_in);
  fclose(fp_out);

  free(p_src_buf);
  free(p_dst_buf);
}

/*---------------------------------------------------------------------------
**
** NAME:           main
**
** DESCRIPTION:    Entry point of the framework.
**
** REMARKS:        Parse the command line arguments and compress the
**                 chunks. 
**
**                 A summary is printed at the end.
**
** AUTHOR:         M. Grosberg   
** CREATED:        Jun 23, 2002
** LAST MODIFIED:  Jul 11, 2002  BY:  MYG (M1027: Remove mode)
**
**------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
  char *p_short_size;
  
  argv0 = argv[0];

  if (argc != 4)
  {
    fprintf(stderr, 
            "Usage:\n\t%s <chunk-size> <input-file> <output-file>\n",
            argv[0]);
            
    return (EXIT_FAILURE);
  }
  
  p_short_size = getenv("HTTPD_USHRT_SZ");
  if (p_short_size == NULL)
    short_size = 2; /* Sensible default. */
  else
  {
    if (strcmp(p_short_size, "host") == 0)
      short_size = sizeof(unsigned short);
    else
      short_size = strtoul(p_short_size, NULL, 0);
    
    if ((short_size == 0) || (short_size > 64))
    {
      fprintf(stderr,
              "%s: Nonsensical value for HTTPD_USHRT_SZ (%s)\n", 
              argv[0],
              p_short_size);
      return (EXIT_FAILURE);
    }
  }  
  
  chunk_size = strtoul(argv[1], NULL, 0);
  if (chunk_size > 65528)
  {
    fprintf(stderr, 
            "%s: Chunk size exceeded maximum allowable 65528.\n", 
            argv[0]);
    return (EXIT_FAILURE);
  }
  p_src_buf = (unsigned char *)malloc(chunk_size);
  if (p_src_buf == NULL)
  {
    fprintf(stderr, 
            "%s: Out of memory allocating source buffer.\n",
            argv[0]);
    return (EXIT_FAILURE);
  }
  p_dst_buf = (unsigned char *)malloc(chunk_size + EXTRA_BLOCK_SIZE);
  if (p_dst_buf == NULL)
  {
    free(p_src_buf);
    fprintf(stderr,
            "%s: Out of memory allocating target buffer.\n",
            argv[0]);
    return (EXIT_FAILURE);
  }

  fp_in = fopen(argv[2], "rb");
  if (fp_in == NULL)
  {
    free(p_src_buf);
    free(p_dst_buf);
    fprintf(stderr,
            "%s: Can't open %s for reading (%s)\n",
            argv[0],
            argv[2],
            strerror(errno));
    return (EXIT_FAILURE);
  }
  
  fp_out = fopen(argv[3], "ab+");
  if (fp_out == NULL)
  {
    free(p_src_buf);
    free(p_dst_buf);
    fprintf(stderr, 
            "%s: Can't open %s for appending (%s)\n",
            argv[0],
            argv[3],
            strerror(errno));
    fclose(fp_in);
    return (EXIT_FAILURE);
  }
  
  atexit(CleanupRoutine);
  CompressChunks();

  if (output_total < input_total)
    printf("%s: Compressed %s from %lu to %lu bytes (%lu%%)\n",
           argv[0],
           argv[2],
           input_total,
           output_total,
           (output_total * 100UL) / input_total);
  else
    printf("%s: Size has increased for file %s by %lu bytes\a\n",
           argv[0],
           argv[2],
           output_total - input_total);

  return (EXIT_SUCCESS);
}

