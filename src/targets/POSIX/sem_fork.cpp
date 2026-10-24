//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_fork.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Implementation of tasking model using processes.
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
//---------------------------------------------------------------------------

#include "seminole.h"

#if defined(HTTPD_USE_FORK)

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::JoinAll
//
// ACCESS:         public
//
// DESCRIPTION:    Join all threads spawned with Fork.
//
// REMARKS:        In this case, it is really not possible to wait 
//                 for all threads to complete. However, since we 
//                 are operating in different memory spaces, it is 
//                 acceptable to be a no-op.
//
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdOpSys::JoinAll()
{}


//---------------------------------------------------------------------------
//
// NAME:           ChildCollector
//
// DESCRIPTION:    Clean up zombies as they form.
//
// REMARKS:        Signals denoting child process termination are sent
//                 to this routine. Since the exit code is relatively
//                 unimportant to Seminole, we just eat the status
//                 code and remove the zombie.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static void ChildCollector(int)
{
  int  status;

  wait(&status);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::Fork
//
// ACCESS:         public
//
// DESCRIPTION:    Spawn a new thread. 
//
// RETURNS:        True if the thread was spawned, false if there was 
//                 an error.
//
// REMARKS:        Three parameters are passed to the newly created
//                 thread in the form of HttpdParameter. 
//
//                 Okay, this is such a cheesy hack, we use child processes 
//                 to handle web requests so we have to collect them here. 
//                 But for systems without Pthreads, this is an acceptable 
//                 simulation for development, not deployment.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
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
  struct sigaction act;

# ifndef SA_RESTART
#  define SA_RESTART   0
# endif

  sigemptyset(&act.sa_mask);
  act.sa_flags   = SA_RESTART;
  act.sa_handler = ChildCollector;
  (void )sigaction(SIGCHLD, &act, NULL);
    
  pid_t pid = fork();

  if (pid < 0)
    return (false);
  else if (pid == 0)
  {
    p_func(p1, p2, p3);    
    _exit(0);
  }
  else
    return (true);
}

#endif

