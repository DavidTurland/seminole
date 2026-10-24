//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_thread.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Implementation of tasking model using VxWorks threads.
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
//  002 |06/22/02| Add thread limiting.                     | MYG  | M1018
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

#include <taskLib.h>
#include <semLib.h>

// This constant bounds the maximum number of thread objects
// in use by Seminole. If this limit is reached, further
// processing may result in errors if a thread is not freed
// up in MAX_WAIT_FREE_TASK ticks. 
const unsigned long MAX_TASKS          = 24;
const unsigned long MAX_WAIT_FREE_TASK = 400;
const unsigned long HTTPD_WAIT_INCR    = 40;

static SEM_ID        runCountLock;
static bool          runInitialized = false;
static bool          runLockout = false;
static unsigned long runCount; 

typedef int VxThreadArgs[10];

union VxParams
{
  struct
  {
    HttpdParameter   args[3];
    void           (*func)
        (HttpdParameter p1, HttpdParameter p2, HttpdParameter p3);
  } ap;
  VxThreadArgs     dummy;
};

//---------------------------------------------------------------------------
//
// NAME:           InitializeThreads
//
// DESCRIPTION:    Initialize the tasking environment.
//
// RETURNS:        True on success, false on failure.
//
// REMARKS:        Just set up some global objects for the abstraction
//                 layer to use. 
//
//                 These objects are never destroyed once created. 
//                 This is not a real issue since they are reused
//                 if the webserver object is stopped and then started
//                 again.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static bool InitializeThreads()
{
  // A performance hack that allows us to transport
  // data over the thread parameters without doing a bunch
  // of synchronization ops is to use the VxParams union.
  // 
  // However, we have to make sure it fits within the required size.
  // If this assert fires, then you have to figure out some other
  // way to pass parameters (see the Pthreads implementation).
  assert(sizeof(VxParams) == sizeof(VxThreadArgs));

  runCountLock = semMCreate(SEM_Q_FIFO);
  if (runCountLock == NULL)
    return (false);

  return (true); 
}

//---------------------------------------------------------------------------
//
// NAME:           ThreadGate
//
// DESCRIPTION:    Callback procedure for VxWorks.
//
// REMARKS:        This routine formats the parameters from the
//                 VxWorks "packet" and calls the actual thread
//                 function.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static void ThreadGate(int a0, int a1, int a2, int a3, int a4,
                       int a5, int a6, int a7, int a8, int a9)
{
  VxParams  p;

  p.dummy[0] = a0; p.dummy[1] = a1; 
  p.dummy[2] = a2; p.dummy[3] = a3;
  p.dummy[4] = a4; p.dummy[5] = a5; 
  p.dummy[6] = a6; p.dummy[7] = a7;
  p.dummy[8] = a8; p.dummy[9] = a9;
  
  (p.ap.func)(p.ap.args[0], p.ap.args[1], p.ap.args[2]);

  semTake(runCountLock, WAIT_FOREVER);
  runCount--;
  semGive(runCountLock);
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
//                 It is expected that all system calls provided by
//                 HttpdOpSys are reentrant and thread-safe.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  Jun 22, 2002  BY:  MYG (M1018: Improve quotas)
//
//--------------------------------------------------------------------------
bool HttpdOpSys::Fork(void (*p_func)(HttpdParameter p1, 
                                     HttpdParameter p2,
                                     HttpdParameter p3),
                      HttpdParameter p1,
                      HttpdParameter p2,
                      HttpdParameter p3)
{
  if (!runInitialized)
  {
    runInitialized = InitializeThreads();
    if (!runInitialized)
      return (false);
  }

  if (runLockout)
    return (false);

#if defined(HTTPD_MAX_THREAD_USAGE) && defined(HTTPD_MAX_WAIT_FREE_TASK)
#if !defined(HTTPD_MAX_WAIT_FREE_TASK_INCR)
#  error HTTPD_MAX_WAIT_FREE_TASK_INCR must be defined.
#endif
  // Quota check.
  bool limit_reached;
  for(unsigned long wt = 0; 
      wt <  HTTPD_MAX_WAIT_FREE_TASK; 
      wt += HTTPD_WAIT_INCR)
  {
    semTake(runCountLock, WAIT_FOREVER);
    limit_reached = (runCount > HTTPD_MAX_THREAD_USAGE);
    if (!limit_reached)
      runCount++;
    semGive(runCountLock);

    if (limit_reached)
      taskDelay((sysClkRateGet() * HTTPD_MAX_WAIT_FREE_TASK_INCR) / 1000);
    else
      break;
  }

  // If we couldn't get past the quota after blocking for a little 
  // while, give up. The request will get denied. 
  if (limit_reached)
    return (false); 
#elif defined(HTTPD_MAX_THREAD_USAGE)
  semTake(runCountLock, WAIT_FOREVER);
  if (runCount > HTTPD_MAX_THREAD_USAGE)
  {
    semGive(runCountLock);
    return (false);
  }
  runCount++;
  semGive(runCountLock);
#else
  semTake(runCountLock, WAIT_FOREVER);
  runCount++;
  semGive(runCountLock);
#endif

  // Spawn the task.
  VxParams p;

  p.ap.args[0] = p1;
  p.ap.args[1] = p2;
  p.ap.args[2] = p3;
  p.ap.func    = p_func;

  int tid = taskSpawn("httpd",
                      210,
                      0,
                      10240,
                      (FUNCPTR )ThreadGate,
                      p.dummy[0], p.dummy[1],
                      p.dummy[2], p.dummy[3],
                      p.dummy[4], p.dummy[5],
                      p.dummy[6], p.dummy[7],
                      p.dummy[8], p.dummy[9]);

  if (tid == ERROR)
  {
    semTake(runCountLock, WAIT_FOREVER);
    runCount--;
    semGive(runCountLock);

    return (false);
  }
  else
    return (true);
}

//---------------------------------------------------------------------------
//
// NAME:           HttpdOpSys::JoinAll
//
// ACCESS:         public
//
// DESCRIPTION:    Join all threads spawned with Fork.
//
// REMARKS:        This method waits for all threads spawned with 
//                 Fork to die off. Threads can not be created
//                 during this call. 
//
//                 In addition, it only makes sense to call this if
//                 no more threads will be spawned (at least, relative
//                 to HttpdOpSys::Fork).
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
void HttpdOpSys::JoinAll()
{
   bool gone  = false;
   bool first = true;

   runLockout = true;

   while (!gone)
   {
     if (first)
       first = false;
     else
       taskDelay(64);

     semTake(runCountLock, WAIT_FOREVER);
     gone = (runCount == 0);
     semGive(runCountLock);
   }

   runLockout = false;
}
