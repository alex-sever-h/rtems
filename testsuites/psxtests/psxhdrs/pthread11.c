/*
 *  This test file is used to verify that the header files associated with
 *  the callout are correct.
 *
 *  COPYRIGHT (c) 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996.
 *  On-Line Applications Research Corporation (OAR).
 *  All rights assigned to U.S. Government, 1994.
 *
 *  This material may be reproduced by or for the U.S. Government pursuant
 *  to the copyright license under the clause at DFARS 252.227-7013.  This
 *  notice must appear in all copies of this file and its derivatives.
 *
 *  $Id$
 */

#include <pthread.h>
 
#ifndef _POSIX_THREAD_PRIORITY_SCHEDULING
#error "RTEMS is support to have pthread_setschedparam"
#endif

void test( void )
{
  pthread_t           thread;
  int                 policy;
  struct sched_param  param;
  int                 status;

  thread = 0;

  policy = SCHED_OTHER;
  policy = SCHED_FIFO;
  policy = SCHED_RR;

  /*
   *  really should use sched_get_priority_min() and sched_get_priority_max()
   */

  param.sched_priority = 0; 

  status = pthread_setschedparam( thread, policy, &param );
}
