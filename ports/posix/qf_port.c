/**
* @file
* @brief QF/C port to POSIX/P-threads, GNU-C compiler
* @ingroup ports
* @cond
******************************************************************************
* Last Updated for Version: 5.6.5
* Date of the Last Update:  2016-06-08
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) Quantum Leaps, LLC. All rights reserved.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Alternatively, this program may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GNU General Public License and are specifically designed for
* licensees interested in retaining the proprietary status of their code.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
* Contact information:
* http://www.state-machine.com
* mailto:info@state-machine.com
******************************************************************************
* @endcond
*/
#define QP_IMPL           /* this is QP implementation */
#include "qf_port.h"      /* QF port */
#include "../../source/qf_pkg.h"
#include "../../include/qassert.h"
#ifdef Q_SPY              /* QS software tracing enabled? */
    #include "qs_port.h"  /* include QS port */
#else
    #include "../../include/qs_dummy.h" /* disable the QS software tracing */
#endif /* Q_SPY */

#include <limits.h>       /* for PTHREAD_STACK_MIN */
#include <sys/mman.h>     /* for mlockall() */

Q_DEFINE_THIS_MODULE("qf_port")

/* Global objects ----------------------------------------------------------*/
pthread_mutex_t QF_pThreadMutex_;

/* Local objects -----------------------------------------------------------*/
static bool l_isRunning;
static struct timespec l_tick;
enum { NANOSLEEP_NSEC_PER_SEC = 1000000000 }; /* see NOTE05 */

/*..........................................................................*/
void QF_init(void) {
    extern uint_fast8_t QF_maxPool_;
    extern QTimeEvt QF_timeEvtHead_[QF_MAX_TICK_RATE];

    /* lock memory so we're never swapped out to disk */
    /*mlockall(MCL_CURRENT | MCL_FUTURE);  uncomment when supported */

    /* init the global mutex with the default non-recursive initializer */
    pthread_mutex_init(&QF_pThreadMutex_, NULL);

    /* clear the internal QF variables, so that the framework can (re)start
    * correctly even if the startup code is not called to clear the
    * uninitialized data (as is required by the C Standard).
    */
    QF_maxPool_ = (uint_fast8_t)0;
    QF_bzero(&QF_timeEvtHead_[0], (uint_fast16_t)sizeof(QF_timeEvtHead_));
    QF_bzero(&QF_active_[0],      (uint_fast16_t)sizeof(QF_active_));

    l_tick.tv_sec = 0;
    l_tick.tv_nsec = NANOSLEEP_NSEC_PER_SEC/100L; /* default clock tick */
}
/*..........................................................................*/
int_t QF_run(void) {
    struct sched_param sparam;

    QF_onStartup();  /* invoke startup callback */

    /* try to maximize the priority of the ticker thread, see NOTE01 */
    sparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sparam) == 0) {
        /* success, this application has sufficient privileges */
    }
    else {
        /* setting priority failed, probably due to insufficient privieges */
    }

    l_isRunning = true;
    while (l_isRunning) { /* the clock tick loop... */
        QF_onClockTick(); /* clock tick callback (must call QF_TICK_X()) */

        nanosleep(&l_tick, NULL); /* sleep for the number of ticks, NOTE05 */
    }
    QF_onCleanup(); /* invoke cleanup callback */
    pthread_mutex_destroy(&QF_pThreadMutex_);

    return (int_t)0; /* return success */
}
/*..........................................................................*/
void QF_setTickRate(uint32_t ticksPerSec) {
    l_tick.tv_nsec = NANOSLEEP_NSEC_PER_SEC / ticksPerSec;
}
/*..........................................................................*/
void QF_stop(void) {
    l_isRunning = false; /* stop the loop in QF_run() */
}
/*..........................................................................*/
static void *thread_routine(void *arg) { /* the expected POSIX signature */
    QActive *act = (QActive *)arg;
    /* loop until m_thread is cleared in QActive_stop() */
    do {
        QEvt const *e = QActive_get_(act); /* wait for the event */
        QMSM_DISPATCH(&act->super, e);     /* dispatch to the SM */
        QF_gc(e);    /* check if the event is garbage, and collect it if so */
    } while (act->thread != (uint8_t)0);
    QF_remove_(act); /* remove this object from any subscriptions */
    pthread_cond_destroy(&act->osObject); /* cleanup the condition variable */
    return (void *)0; /* return success */
}
/*..........................................................................*/
void QActive_start_(QActive * const me, uint_fast8_t prio,
                    QEvt const *qSto[], uint_fast16_t qLen,
                    void *stkSto, uint_fast16_t stkSize,
                    QEvt const *ie)
{
    pthread_t thread;
    pthread_attr_t attr;
    struct sched_param param;

    /* p-threads allocate stack internally */
    Q_REQUIRE_ID(600, stkSto == (void *)0);

    QEQueue_init(&me->eQueue, qSto, qLen);
    pthread_cond_init(&me->osObject, 0);

    me->prio = (uint8_t)prio;
    QF_add_(me); /* make QF aware of this active object */

    QMSM_INIT(&me->super, ie); /* take the top-most initial tran. */
    QS_FLUSH(); /* flush the QS trace buffer to the host */

    pthread_attr_init(&attr);

    /* SCHED_FIFO corresponds to real-time preemptive priority-based scheduler
    * NOTE: This scheduling policy requires the superuser privileges
    */
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    /* see NOTE04 */
    param.sched_priority = prio
                           + (sched_get_priority_max(SCHED_FIFO)
                              - QF_MAX_ACTIVE - 3);

    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (stkSize == 0U) {
        /* set the allowed minimum */
        stkSize = (uint_fast16_t)PTHREAD_STACK_MIN;
    }
    pthread_attr_setstacksize(&attr, (size_t)stkSize);

    if (pthread_create(&thread, &attr, &thread_routine, me) != 0) {
        /* Creating the p-thread with the SCHED_FIFO policy failed. Most
        * probably this application has no superuser privileges, so we just
        * fall back to the default SCHED_OTHER policy and priority 0.
        */
        pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
        param.sched_priority = 0;
        pthread_attr_setschedparam(&attr, &param);
        Q_ALLEGE(pthread_create(&thread, &attr, &thread_routine, me)== 0);
    }
    pthread_attr_destroy(&attr);
    me->thread = (uint8_t)1;
}
/*..........................................................................*/
void QActive_stop(QActive *me) {
    me->thread = (uint8_t)0; /* stop the QActive thread loop */
}

/*****************************************************************************
* NOTE01:
* In Linux, the scheduler policy closest to real-time is the SCHED_FIFO
* policy, available only with superuser privileges. QF_run() attempts to set
* this policy as well as to maximize its priority, so that the ticking
* occurrs in the most timely manner (as close to an interrupt as possible).
* However, setting the SCHED_FIFO policy might fail, most probably due to
* insufficient privileges.
*
* NOTE02:
* On some Linux systems nanosleep() might actually not deliver the finest
* time granularity. For example, on some Linux implementations, nanosleep()
* could not block for shorter intervals than 20ms, while the underlying
* clock tick period was only 10ms. Sometimes, the select() system call can
* provide a finer granularity.
*
* NOTE03:
* Any blocking system call, such as nanosleep() or select() system call can
* be interrupted by a signal, such as ^C from the keyboard. In this case this
* QF port breaks out of the event-loop and returns to main() that exits and
* terminates all spawned p-threads.
*
* NOTE04:
* According to the man pages (for pthread_attr_setschedpolicy) the only value
* supported in the Linux p-threads implementation is PTHREAD_SCOPE_SYSTEM,
* meaning that the threads contend for CPU time with all processes running on
* the machine. In particular, thread priorities are interpreted relative to
* the priorities of all other processes on the machine.
*
* This is good, because it seems that if we set the priorities high enough,
* no other process (or thread running within) can gain control over the CPU.
*
* However, QF limits the number of priority levels to QF_MAX_ACTIVE.
* Assuming that a QF application will be real-time, this port reserves the
* three highest Linux priorities for the ISR-like threads (e.g., the ticker,
* I/O), and the rest highest-priorities for the active objects.
*
* NOTE05:
* In some (older) Linux kernels, the POSIX nanosleep() system call might
* deliver only 2*actual-system-tick granularity. To compensate for this,
* you would need to reduce (by 2) the constant NANOSLEEP_NSEC_PER_SEC.
*/

