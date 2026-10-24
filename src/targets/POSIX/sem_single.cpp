//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_single.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Implementation of tasking model using stubs.
//
//                  This is mainly a tool for debugging, but can be 
//                  useful where serialization is required.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Nov  6, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |11/06/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

#if defined(HTTPD_USE_SINGLE)

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::JoinAll
//
// ACCESS:         public
//
// DESCRIPTION:    Join all threads
//
// REMARKS:        There are no threads, therefore this routine
//                 is a no-op.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Nov  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdOpSys::JoinAll()
{}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Fork
//
// ACCESS:         public
//
// DESCRIPTION:    Spawn a new thread (sort of). 
//
// RETURNS:        True if the thread was spawned, false if there was 
//                 an error.
//
// REMARKS:        Three parameters are passed to the newly created
//                 "thread" in the form of HttpdParameter. 
//
//                 Technically, this is a form of multi-tasking. It
//                 just so happens that all threads are scheduled 
//                 until completion. Well, you can look at it that way. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Nov  6, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
bool HttpdOpSys::Fork(void (*p_func)(HttpdParameter p1, 
                                     HttpdParameter p2,
                                     HttpdParameter p3),
                      HttpdParameter p1,
                      HttpdParameter p2,
                      HttpdParameter p3)
{
   static bool thread_initialized = false;

   if (!thread_initialized)
   {
     struct sigaction act;

# ifndef SA_RESTART
#  define SA_RESTART   0
# endif

     sigemptyset(&act.sa_mask);
     act.sa_flags   = SA_RESTART;
     act.sa_handler = SIG_IGN;

     // Some of these dangerous signals will terminate a process.
     // Just ignore them.
     sigaction(SIGPIPE, &act, NULL);
     sigaction(SIGURG, &act, NULL);

     thread_initialized = true;
   }

   p_func(p1, p2, p3);    
   return (true);
}

#endif

