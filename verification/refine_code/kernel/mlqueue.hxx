#ifndef CYGONCE_KERNEL_MLQUEUE_HXX
#define CYGONCE_KERNEL_MLQUEUE_HXX

//==========================================================================
//
//      mlqueue.hxx
//
//      Multi-Level Queue scheduler class declarations
//
//==========================================================================
// ####ECOSGPLCOPYRIGHTBEGIN####                                            
// -------------------------------------------                              
// This file is part of eCos, the Embedded Configurable Operating System.   
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under    
// the terms of the GNU General Public License as published by the Free     
// Software Foundation; either version 2 or (at your option) any later      
// version.                                                                 
//
// eCos is distributed in the hope that it will be useful, but WITHOUT      
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or    
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License    
// for more details.                                                        
//
// You should have received a copy of the GNU General Public License        
// along with eCos; if not, write to the Free Software Foundation, Inc.,    
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.            
//
// As a special exception, if other files instantiate templates or use      
// macros or inline functions from this file, or you compile this file      
// and link it with other works to produce a work based on this file,       
// this file does not by itself cause the resulting work to be covered by   
// the GNU General Public License. However the source code for this file    
// must still be made available in accordance with section (3) of the GNU   
// General Public License v2.                                               
//
// This exception does not invalidate any other reasons why a work based    
// on this file might be covered by the GNU General Public License.         
// -------------------------------------------                              
// ####ECOSGPLCOPYRIGHTEND####                                              
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    nickg
// Contributors: jlarmour
// Date:         1997-09-10
// Purpose:      Define multilevel queue scheduler implementation
// Description:  The classes defined here are used as base classes
//               by the common classes that define schedulers and thread
//               things. The MLQ scheduler in various configurations
//               provides standard FIFO, round-robin and single priority
//               schedulers.
// Usage:        Included according to configuration by
//               <cyg/kernel/sched.hxx>
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <cyg/kernel/ktypes.h>

#include <cyg/infra/clist.hxx>                  // List implementation

// -------------------------------------------------------------------------
// The macro CYGNUM_KERNEL_SCHED_PRIORITIES contains the number of priorities
// supported by the scheduler.

#ifndef CYGNUM_KERNEL_SCHED_PRIORITIES
#define CYGNUM_KERNEL_SCHED_PRIORITIES 32       // define a default
#endif

// set bitmap size
#define CYGNUM_KERNEL_SCHED_BITMAP_SIZE CYGNUM_KERNEL_SCHED_PRIORITIES

// -------------------------------------------------------------------------
// The macro CYGNUM_KERNEL_SCHED_BITMAP_SIZE contains the number of bits that the
// scheduler bitmap should contain. It is derived from the number of prioirity
// levels defined by the configuration.

//#elif CYGNUM_KERNEL_SCHED_BITMAP_SIZE <= 32
typedef cyg_ucount32 cyg_sched_bitmap;

// -------------------------------------------------------------------------
// Customize the scheduler

#define CYGIMP_THREAD_PRIORITY  1       // Threads have changable priorities

#define CYG_THREAD_MIN_PRIORITY (CYGNUM_KERNEL_SCHED_PRIORITIES-1)
#define CYG_THREAD_MAX_PRIORITY 0

// set default scheduling info value for thread constructors.
#define CYG_SCHED_DEFAULT_INFO  CYG_THREAD_MAX_PRIORITY

// -------------------------------------------------------------------------
// scheduler Run queue object

typedef Cyg_CList_T<Cyg_Thread> Cyg_RunQueue;

// -------------------------------------------------------------------------
// Thread queue implementation.
// This class provides the (scheduler specific) implementation of the
// thread queue class.

class Cyg_ThreadQueue_Implementation
    : public Cyg_CList_T<Cyg_Thread>
{
    friend class Cyg_Scheduler_Implementation;
    friend class Cyg_SchedThread_Implementation;

    void                set_thread_queue(Cyg_Thread *thread,
                                         Cyg_ThreadQueue *tq );

protected:

    // API used by Cyg_ThreadQueue

    Cyg_ThreadQueue_Implementation() {};   // Constructor
    
                                        // Add thread to queue
    void                enqueue(Cyg_Thread *thread);

                                        // return first thread on queue
    Cyg_Thread          *highpri();

                                        // remove first thread on queue    
    Cyg_Thread          *dequeue();

                                        // Remove thread from queue
    void                remove(Cyg_Thread *thread);

};

// -------------------------------------------------------------------------
// This class contains the implementation details of the scheduler, and
// provides a standard API for accessing it.

class Cyg_Scheduler_Implementation
    : public Cyg_Scheduler_Base
{
    friend class Cyg_ThreadQueue_Implementation;
    friend class Cyg_SchedThread_Implementation;
    friend class Cyg_HardwareThread;
    friend void cyg_scheduler_set_need_reschedule();
    
    // Mask of which run queues have ready threads
    cyg_sched_bitmap    queue_map;

    // Each run queue is a double linked circular list of threads.
    // These pointers point to the head element of each list.
    Cyg_RunQueue run_queue[CYGNUM_KERNEL_SCHED_PRIORITIES];


protected:
    
//#ifdef CYGSEM_KERNEL_SCHED_TIMESLICE

    // Timeslice counter. This is decremented on each
    // clock tick, and a timeslice is performed each
    // time it zeroes.
    
    static cyg_ucount32 timeslice_count[CYGNUM_KERNEL_CPU_MAX]
                                        CYGBLD_ANNOTATE_VARIABLE_SCHED;



    Cyg_Scheduler_Implementation();     // Constructor
    
    // The following functions provide the scheduler implementation
    // interface to the Cyg_Scheduler class. These are protected
    // so that only the scheduler can call them.
    
    // choose a new thread
    Cyg_Thread  *schedule();

    // make thread schedulable
    void        add_thread(Cyg_Thread *thread);

    // make thread un-schedulable
    void        rem_thread(Cyg_Thread *thread);

    // register thread with scheduler
    void        register_thread(Cyg_Thread *thread);

    // deregister thread
    void        deregister_thread(Cyg_Thread *thread);
    
    // Test the given priority for uniqueness
    cyg_bool    unique( cyg_priority priority);

    // Set need_reschedule if the supplied thread is of lower
    // priority than any that are currently running.
    static void set_need_reschedule( Cyg_Thread *thread );
    static void set_need_reschedule();

public:
    void set_idle_thread( Cyg_Thread *thread, HAL_SMP_CPU_TYPE cpu );
    
//#ifdef CYGSEM_KERNEL_SCHED_TIMESLICE

    // If timeslicing is enbled, define a scheduler
    // entry points to do timeslicing. This will be
    // called from the RTC DSR.
public:    
    void timeslice();
    void timeslice_cpu();


};

// -------------------------------------------------------------------------
// Cyg_Scheduler_Implementation inlines

inline void Cyg_Scheduler_Implementation::set_need_reschedule()
{
    need_reschedule[CYG_KERNEL_CPU_THIS()] = true;
}


// -------------------------------------------------------------------------
// Scheduler thread implementation.
// This class provides the implementation of the scheduler specific parts
// of each thread.

class Cyg_SchedThread_Implementation
    : public Cyg_DNode_T<Cyg_Thread>
{
    friend class Cyg_Scheduler_Implementation;
    friend class Cyg_ThreadQueue_Implementation;

protected:

    cyg_priority        priority;       // current thread priority

    
    Cyg_SchedThread_Implementation(CYG_ADDRWORD sched_info);

    void yield();                       // Yield CPU to next thread

    static void rotate_queue( cyg_priority pri );
                                        // Rotate that run queue

    void to_queue_head( void );         // Move this thread to the head
                                        // of its queue (not necessarily
                                        // a scheduler queue)

//#ifdef CYGSEM_KERNEL_SCHED_TIMESLICE

    cyg_ucount32 timeslice_count;
        
    void timeslice_save();

    void timeslice_restore();
    
    void timeslice_reset();
       
};

// -------------------------------------------------------------------------
// Cyg_SchedThread_Implementation inlines.

//#ifdef CYGSEM_KERNEL_SCHED_TIMESLICE

inline void Cyg_SchedThread_Implementation::timeslice_save()
{
    timeslice_count = Cyg_Scheduler_Implementation::timeslice_count[CYG_KERNEL_CPU_THIS()];
}

inline void Cyg_SchedThread_Implementation::timeslice_restore()
{
    Cyg_Scheduler_Implementation::timeslice_count[CYG_KERNEL_CPU_THIS()] = timeslice_count;
}

inline void Cyg_SchedThread_Implementation::timeslice_reset()
{
    timeslice_count = CYGNUM_KERNEL_SCHED_TIMESLICE_TICKS;
}


// -------------------------------------------------------------------------
#endif // ifndef CYGONCE_KERNEL_MLQUEUE_HXX
// EOF mlqueue.hxx