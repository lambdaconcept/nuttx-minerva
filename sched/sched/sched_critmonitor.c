/****************************************************************************
 * sched/sched/sched_critmonitor.c
 *
 *   Copyright (C) 2018 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/types.h>
#include <sched.h>
#include "sched/sched.h"

#ifdef CONFIG_SCHED_CRITMONITOR

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Flags used by the critical section monitor */

#define CRITMON_PREEMPT       (1 << 0)  /* Bit 0:  Pre-emption is disabled */
#define CRITMON_CSECTION      (1 << 1)  /* Bit 1:  In a critical section */

#define DISABLE_PREEMPT(t)    do { (t)->crit_flags |= CRITMON_PREEMPT; } while (0)
#define ENTER_CSECTION(t)     do { (t)->crit_flags |= CRITMON_CSECTION; } while (0)

#define ENABLE_PREEMPT(t)     do { (t)->crit_flags &= ~CRITMON_PREEMPT; } while (0)
#define LEAVE_CSECTION(t)     do { (t)->crit_flags &= ~CRITMON_CSECTION; } while (0)

#define PREEMPT_ISDISABLED(t) (((t)->crit_flags & CRITMON_PREEMPT) != 0)
#define IN_CSECTION(t)        (((t)->crit_flags & CRITMON_CSECTION) != 0)

/****************************************************************************
 * External Function Prototypes
 ****************************************************************************/

/* If CONFIG_SCHED_CRITMONITOR is selected, then platform-specific logic
 * must provide the following interface.  This interface simply provides the
 * current time value in unknown units.  NOTE:  This function may be called
 * early before the timer has been initialized.  In that event, the function
 * should just return a start time of zero.
 *
 * Nothing is assumed about the units of this time value.  The following
 * are assumed, however: (1) The time is an unsigned is an unsigned integer
 * value, (2) is is monotonically increasing, and (3) the elapsed time
 * (also in unknown units) can be obtained by subtracting a start time
 * from the current time.
 */

uint32_t up_critmon_gettime(void);

/************************************************************************************
 * Private Data
 ************************************************************************************/

/* Start time when pre-emption disabled or critical section entered. */

#ifdef CONFIG_SMP_NCPUS
static uint32_t g_premp_start[CONFIG_SMP_NCPUS];
static uint32_t g_crit_start[CONFIG_SMP_NCPUS];
#else
static uint32_t g_premp_start[1];
static uint32_t g_crit_start[1];
#endif

/************************************************************************************
 * Public Data
 ************************************************************************************/

/* Maximum time with pre-emption disabled or within critical section. */

#ifdef CONFIG_SMP_NCPUS
uint32_t g_premp_max[CONFIG_SMP_NCPUS];
uint32_t g_crit_max[CONFIG_SMP_NCPUS];
#else
uint32_t g_premp_max[1];
uint32_t g_crit_max[1];
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sched_critmon_preemption
 *
 * Description:
 *   Called when there is any change in pre-emptible state of a thread.
 *
 * Assumptions:
 *   Called within a critical section.
 *
 ****************************************************************************/

void sched_critmon_preemption(FAR struct tcb_s *tcb, bool state)
{
  int cpu = this_cpu();

  /* Are we enabling or disabling pre-emption */

  if (state)
    {
      DEBUGASSERT(tcb->premp_start == 0 && !PREEMPT_ISDISABLED(tcb));

      /* Disabling.. Save the thread start time */

      tcb->premp_start = up_critmon_gettime();

      /* Zero means that the timer is not ready */

      if (tcb->premp_start != 0)
        {
          DISABLE_PREEMPT(tcb);

          /* Save the global start time */

          g_premp_start[cpu] = tcb->premp_start;
        }
    }
  else if (tcb->premp_start != 0)
    {
      /* Re-enabling.. Check for the max elapsed time */

      uint32_t now     = up_critmon_gettime();
      uint32_t elapsed = now - tcb->premp_start;

      DEBUGASSERT(now != 0 && PREEMPT_ISDISABLED(tcb));

      tcb->premp_start = 0;
      if (elapsed > tcb->premp_max)
        {
          tcb->premp_max = elapsed;
        }

      ENABLE_PREEMPT(tcb);

      /* Check for the global max elapsed time */

      elapsed            = now - g_premp_start[cpu];
      g_premp_start[cpu] = 0;

      if (elapsed > g_premp_max[cpu])
        {
          g_premp_max[cpu] = elapsed;
        }
    }
}

/****************************************************************************
 * Name: sched_critmon_csection
 *
 * Description:
 *   Called when a thread enters or leaves a critical section.
 *
 * Assumptions:
 *   Called within a critical section.
 *
 ****************************************************************************/

void sched_critmon_csection(FAR struct tcb_s *tcb, bool state)
{
  int cpu = this_cpu();

  /* Are we entering or leaving the critical section? */

  if (state)
    {
      /* Entering... Save the start time. */

      DEBUGASSERT(tcb->crit_start == 0);
      tcb->crit_start = up_critmon_gettime();

      /* Zero means that the timer is not ready */

      if (tcb->crit_start != 0)
        {
          ENTER_CSECTION(tcb);

          /* Set the global start time */

          g_crit_start[cpu] = tcb->crit_start;
        }
    }
  else if (tcb->crit_start != 0)
    {
      /* Leaving .. Check for the max elapsed time */

      uint32_t now     = up_critmon_gettime();
      uint32_t elapsed = now - tcb->crit_start;

      DEBUGASSERT(now != 0 && IN_CSECTION(tcb));

      tcb->crit_start = 0;
      if (elapsed > tcb->crit_max)
        {
          tcb->crit_max = elapsed;
        }

      LEAVE_CSECTION(tcb);

      /* Check for the global max elapsed time */

      elapsed           = now - g_crit_start[cpu];
      g_crit_start[cpu] = 0;

      if (elapsed > g_crit_max[cpu])
        {
          g_crit_max[cpu] = elapsed;
        }
    }
}

/****************************************************************************
 * Name: sched_critmon_resume
 *
 * Description:
 *   Called when a thread resumes execution, perhaps re-establishing a
 *   critical section or a non-pre-emptible state.
 *
 * Assumptions:
 *   Called within a critical section.
 *
 ****************************************************************************/

void sched_critmon_resume(FAR struct tcb_s *tcb)
{
  uint32_t elapsed;
  int cpu = this_cpu();

  DEBUGASSERT(tcb->premp_start == 0 && tcb->crit_start == 0);

  /* Did this task disable pre-emption? */

  if (PREEMPT_ISDISABLED(tcb))
    {
      /* Yes.. Save the start time */

      tcb->premp_start = up_critmon_gettime();
      DEBUGASSERT(tcb->premp_start != 0);

      if (g_premp_start[cpu] == 0)
        {
          g_premp_start[cpu] = tcb->premp_start;
        }
    }
  else if (g_premp_start[cpu] != 0)
    {
      /* Check for the global max elapsed time */

      elapsed            = up_critmon_gettime() - g_premp_start[cpu];
      g_premp_start[cpu] = 0;

      if (elapsed > g_premp_max[cpu])
        {
          g_premp_max[cpu] = elapsed;
        }
    }

  /* Was this task in a critical section? */

  if (IN_CSECTION(tcb))
    {
      /* Yes.. Save the start time */

      tcb->crit_start = up_critmon_gettime();
      DEBUGASSERT(tcb->crit_start != 0);

      if (g_crit_start[cpu] == 0)
        {
          g_crit_start[cpu] = tcb->crit_start;
        }
    }
  else if (g_crit_start[cpu] != 0)
    {
      /* Check for the global max elapsed time */

      elapsed      = up_critmon_gettime() - g_crit_start[cpu];
      g_crit_start[cpu] = 0;

      if (elapsed > g_crit_max[cpu])
        {
          g_crit_max[cpu] = elapsed;
        }
    }
}

/****************************************************************************
 * Name: sched_critmon_suspend
 *
 * Description:
 *   Called when a thread suspends execution, perhaps terminating a
 *   critical section or a non-pre-emptible state.
 *
 * Assumptions:
 *   Called within a critical section.
 *
 ****************************************************************************/

void sched_critmon_suspend(FAR struct tcb_s *tcb)
{
  uint32_t elapsed;

  /* Did this task disable pre-emption? */

  if (PREEMPT_ISDISABLED(tcb))
    {
      /* Possibly re-enabling.. Check for the max elapsed time */

      elapsed = up_critmon_gettime() - tcb->premp_start;

      tcb->premp_start = 0;
      if (elapsed > tcb->premp_max)
        {
          tcb->premp_max = elapsed;
        }
    }

  /* Is this task in a critical section? */

  if (IN_CSECTION(tcb))
    {
      /* Possibly leaving .. Check for the max elapsed time */

      elapsed = up_critmon_gettime() - tcb->crit_start;

      tcb->crit_start = 0;
      if (elapsed > tcb->crit_max)
        {
          tcb->crit_max = elapsed;
        }
    }
}

#endif