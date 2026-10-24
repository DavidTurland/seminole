//---------------------------------------------------------------------------
//
// MODULE NAME:     sem_pthread.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Implementation of tasking model using Pthreads.
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
//  002 |06/02/02| Thread limits.                           | MYG  | M1018
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

#if !defined(HTTPD_USE_FORK) && !defined(HTTPD_USE_SINGLE)

#include <pthread.h>
#include <sys/types.h>
#ifdef HTTPD_USE_POLL
# include <poll.h>
#endif

static bool             runInitialized = false;

static bool             runFinished;
static pthread_mutex_t  runLock     = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  runDoneLck  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  runCountLck = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   runDone     = PTHREAD_COND_INITIALIZER;
static unsigned long    runCount    = 0;
static void           (*runFunc)(HttpdParameter p1,
                                 HttpdParameter p2,
                                 HttpdParameter p3);
static HttpdParameter  runP1;
static HttpdParameter  runP2;
static HttpdParameter  runP3;


//---------------------------------------------------------------------------
//
// NAME:           ThreadGate
//
// DESCRIPTION:    Callback procedure for Pthreads.
//
// REMARKS:        This routine obtains the parameters from the
//                 common exchange area and puts them on the stack
//                 for the actual thread routine.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static void *ThreadGate(void *p_args)
{
  HttpdParameter  p1, p2, p3;
  void           (*func)(HttpdParameter p1,
                         HttpdParameter p2,
                         HttpdParameter p3);

  p1   = runP1;
  p2   = runP2;
  p3   = runP3;
  func = runFunc;

  // Signal the parent thread we are done. 
  pthread_mutex_lock(&runDoneLck);
  runFinished = true;
  pthread_cond_signal(&runDone);
  pthread_mutex_unlock(&runDoneLck);

  // Call thread procedure.
  func(p1, p2, p3);

  // Decrement us from the run count.
  pthread_mutex_lock(&runCountLck);
  runCount--;
  pthread_mutex_unlock(&runCountLck);

  return (NULL);
}

//---------------------------------------------------------------------------
//
// NAME:           PthreadsSetup
//
// DESCRIPTION:    Initialize the Pthreads environment.
//
// REMARKS:        Apparently, these signals can cause process termination
//                 which would kill all threads in the process. 
//
//                 Odd that the application programmer needs to take
//                 care of setting these up to "sane" handlers.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Oct 30, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static void PthreadsSetup()
{
  struct sigaction act;

# ifndef SA_RESTART
#  define SA_RESTART   0
# endif

  sigemptyset(&act.sa_mask);
  act.sa_flags   = SA_RESTART;
  act.sa_handler = SIG_IGN;

  // Some of these dangerous signals will terminate a process.
  //
  // If a single thread causes us to receive them we should just
  // ignore them. This is especially important for SIGPIPE which
  // BSD sockets likes to generate frequently.
  sigaction(SIGPIPE, &act, NULL);
  sigaction(SIGURG, &act, NULL);
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
   pthread_mutex_lock(&runLock);

   bool gone  = false;
   bool first = true;

   while (!gone)
   {
     if (first)
       first = false;
     else
       sleep(1);

     pthread_mutex_lock(&runCountLck);
     gone = (runCount == 0);
     pthread_mutex_unlock(&runCountLck);
   }

   pthread_mutex_unlock(&runLock);
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
// LAST MODIFIED:  Jun  2, 2002  BY:  MYG (M1018: Thread limits)
//
//--------------------------------------------------------------------------
bool HttpdOpSys::Fork(void (*p_func)(HttpdParameter p1, 
                                     HttpdParameter p2,
                                     HttpdParameter p3),
                      HttpdParameter p1,
                      HttpdParameter p2,
                      HttpdParameter p3)
{
   pthread_t  thread;
   bool       result;

   // If this is our first time, initialize some semantics.
   if (!runInitialized)
   {
      PthreadsSetup();
      runInitialized = true;
   }

   // We have to lock out other callers to protect the Thread 
   // Communication Area since it is a bunch of shared globals.
   //
   // In addition, we need a second mutex and condition variable
   // to tell us when the spawned thread is done with the global
   // area so we can let other threads continue on.
   pthread_mutex_lock(&runLock);

   runP1       = p1;
   runP2       = p2;
   runP3       = p3;
   runFunc     = p_func;
   runFinished = false;

#if defined(HTTPD_MAX_THREAD_USAGE) && defined(HTTPD_MAX_WAIT_FREE_TASK)
#if !defined(HTTPD_MAX_WAIT_FREE_TASK_INCR)
#  error HTTPD_MAX_WAIT_FREE_TASK_INCR must be defined.
#endif
   // Quota check.
   bool limit_reached;
   for(unsigned long wt = 0; 
       wt <  HTTPD_MAX_WAIT_FREE_TASK; 
       wt += HTTPD_MAX_WAIT_FREE_TASK_INCR)
   {
     pthread_mutex_lock(&runCountLck);
     limit_reached = (runCount > HTTPD_MAX_THREAD_USAGE);
     if (!limit_reached)
       runCount++;
     pthread_mutex_unlock(&runCountLck);
 
     if (limit_reached)
     {
#if defined(HTTPD_USE_POLL)
       poll(NULL, 0, HTTPD_MAX_WAIT_FREE_TASK_INCR);
#else /* Use select. */
       struct timeval  tv;

       tv.tv_sec  = HTTPD_MAX_WAIT_FREE_TASK_INCR / 1000;
       tv.tv_usec = (HTTPD_MAX_WAIT_FREE_TASK_INCR % 1000) * 1000; 
       select(1, NULL, NULL, NULL, &tv);
#endif
     }
     else
       break;
   }

   // If we couldn't get past the quota after blocking for a little 
   // while, give up. The request will get denied. 
   if (limit_reached)
   {
     pthread_mutex_unlock(&runLock);
     return (false);
   }
#elif defined(HTTPD_MAX_THREAD_USAGE)
   // Increment the count.
   pthread_mutex_lock(&runCountLck);
   if (runCount > HTTPD_MAX_THREAD_USAGE)
   {
     pthread_mutex_unlock(&runCountLck);
     pthread_mutex_unlock(&runLock);
     return (false);
   }
   runCount++;
   pthread_mutex_unlock(&runCountLck);
#else
   pthread_mutex_lock(&runCountLck);
   runCount++;
   pthread_mutex_unlock(&runCountLck);
#endif

   if (pthread_create(&thread, NULL, ThreadGate, NULL) == 0)
   {
     pthread_detach(thread);

     // See if we are done. The while () loop is necessary for
     // two reasons:
     //
     //   (1) Pthreads does not guarantee against spurious wake-up.
     //   (2) The spawned thread may have actually signaled the 
     //       condition variable before we get here. Sadly, it is 
     //       not a counted semaphore. Doesn't that just suck? 
     //
     pthread_mutex_lock(&runDoneLck);
     while (!runFinished)
           pthread_cond_wait(&runDone, &runDoneLck);
     pthread_mutex_unlock(&runDoneLck);
     result = true;
   }
   else
   {
     pthread_mutex_lock(&runCountLck);
     runCount--;
     pthread_mutex_unlock(&runCountLck);

     result = false;
   }

   pthread_mutex_unlock(&runLock);
   return (result);
}

#endif

