//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_opsys.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Operating System Abstraction (VxWorks)
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct 30, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/30/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |01/22/02| New filesystem abstraction.              | MYG  |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"
#include <memLib.h>

/****************************************************************************/
/*                            MEMORY ABSTRACTION                            */
/****************************************************************************/

#ifdef HTTPD_MAX_HEAP_USAGE

static PART_ID  memPart = NULL;

//---------------------------------------------------------------------------
//
// NAME:           GetMemory
//
// DESCRIPTION:    Initialize the Seminole memory partition.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        The partition is never destroyed once created. 
//                 This is not a real issue since it is reused if
//                 the webserver object is stopped and then started
//                 again.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static bool GetMemory()
{
  char *p_buffer = malloc(HTTPD_MAX_HEAP_USAGE);
  if (p_buffer == NULL)
    return (false);

  memPart = memPartCreate((char *)p_buffer, HTTPD_MAX_HEAP_USAGE);
  if (memPart == NULL)
  {
    free(p_buffer);
    return (false);
  }

  return (true);
}

#endif

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Malloc
//
// ACCESS:         public
//
// DESCRIPTION:    Allocate uninitialized memory.
//
// RETURNS:        A pointer to a buffer of at-least the requested
//                 size aligned to allow the most restrictive type
//                 to exist at the returned pointer. 
//
//                 NULL is returned on error.
//
// REMARKS:        The memory is allocated from the Seminole pool. 
//                 This way a hard limit can be placed on the resources
//                 that Seminole uses (this is typically the way most
//                 embedded systems would use it).
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void *HttpdOpSys::Malloc(size_t sz)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  if (memPart == NULL)
  {
    if (!GetMemory())
      return (NULL);
  }
  
  return memPartAlloc(memPart, sz);
#else
  return malloc(sz);
#endif
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Free
//
// ACCESS:         public
//
// DESCRIPTION:    Release allocated memory.
//
// REMARKS:        Some memory must have been allocated to make sure the
//                 pool exists. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void HttpdOpSys::Free(void *p_ptr)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  assert(memPart != NULL);

  if (p_ptr != NULL)
    memPartFree(memPart, p_ptr);
#else
  free(p_ptr);
#endif
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Realloc
//
// ACCESS:         public
//
// DESCRIPTION:    Change the size of an allocated memory block.
//
// RETURNS:        A pointer to the new memory block on success
//                 and NULL on failure (the original block is no longer
//                 valid on failure). 
//
// REMARKS:        Behaves like the ANSI C function realloc() with a 
//                 twist: the original block will not be valid on failure.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  Jun  1, 2002  BY:  MYG (M1017: Memory limiting)
//
//--------------------------------------------------------------------------
void *HttpdOpSys::Realloc(void *p_oldptr, size_t newsz)
{
#ifdef HTTPD_MAX_HEAP_USAGE
  if (newsz == 0)
  {
    Free(p_oldptr);
    return (NULL);
  }

  // Since we may have a malloc() type operation, get memory...
  if (memPart == NULL)
  {
    if (!GetMemory())
      return (NULL);
  }
   
  // The VxWorks documentation is rather sketchy on the failure modes
  // of this particular routine. 
  //
  // Since I don't have access to the VxWorks sources, I am going to
  // AssUME that it behaves like the ANSI C variant.
  void *p_result = memPartRealloc(memPart, (char *)p_oldptr, newsz);
  
  if (p_result == NULL)
    Free(p_oldptr);

  return (p_result);
#else
  void *p_result = realloc(p_oldptr, newsz);
  
  if (p_result == NULL)
    HttpdOpSys::Free(p_oldptr);

  return (p_result);
#endif
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::NativeFileSystem
//
// ACCESS:         public
//
// DESCRIPTION:    Obtain a pointer to the operating systems
//                 native file manager.
//
// RETURNS:        NULL always. We never use the native VxWorks
//                 file system.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jan 19, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
HttpdFileSystem *HttpdOpSys::NativeFileSystem()
{
  return (NULL);
}
