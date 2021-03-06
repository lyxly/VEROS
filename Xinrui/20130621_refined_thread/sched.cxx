//==========================================================================
//
//      sched/sched.cxx
//
//      Scheduler class implementations
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
// Author(s):   nickg
// Contributors:        nickg
// Date:        1997-09-15
// Purpose:     Scheduler class implementation
// Description: This file contains the definitions of the scheduler class
//              member functions that are common to all scheduler
//              implementations.
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/kernel.h>

#include <cyg/kernel/ktypes.h>         // base kernel types
#include <cyg/infra/cyg_trac.h>        // tracing macros
#include <cyg/infra/cyg_ass.h>         // assertion macros
#include <cyg/kernel/instrmnt.h>       // instrumentation

#include <cyg/kernel/sched.hxx>        // our header

#include <cyg/kernel/thread.hxx>       // thread classes
#include <cyg/kernel/intr.hxx>         // Interrupt interface

#include <cyg/hal/hal_arch.h>          // Architecture specific definitions

#include <cyg/kernel/thread.inl>       // thread inlines
#include <cyg/kernel/sched.inl>        // scheduler inlines

//-------------------------------------------------------------------------
// Some local tracing control - a default.
#ifdef CYGDBG_USE_TRACING
# if !defined( CYGDBG_INFRA_DEBUG_TRACE_ASSERT_SIMPLE ) && \
     !defined( CYGDBG_INFRA_DEBUG_TRACE_ASSERT_FANCY  )
   // ie. not a tracing implementation that takes a long time to output

#  ifndef CYGDBG_KERNEL_TRACE_UNLOCK_INNER
#   define CYGDBG_KERNEL_TRACE_UNLOCK_INNER
#  endif // control not already defined

# endif  // trace implementation not ..._SIMPLE && not ..._FANCY
#endif   // CYGDBG_USE_TRACING

// -------------------------------------------------------------------------
// Static Cyg_Scheduler class members

// We start with sched_lock at 1 so that any kernel code we
// call during initialization will not try to reschedule.

CYGIMP_KERNEL_SCHED_LOCK_DEFINITIONS;

Cyg_Thread              *volatile Cyg_Scheduler_Base::current_thread[CYGNUM_KERNEL_CPU_MAX];

volatile cyg_bool       Cyg_Scheduler_Base::need_reschedule[CYGNUM_KERNEL_CPU_MAX];

Cyg_Scheduler           Cyg_Scheduler::scheduler CYG_INIT_PRIORITY( SCHEDULER );

volatile cyg_ucount32   Cyg_Scheduler_Base::thread_switches[CYGNUM_KERNEL_CPU_MAX];



// -------------------------------------------------------------------------
// Scheduler unlock function.

// This is only called when there is the potential for real work to be
// done. Other cases are handled in Cyg_Scheduler::unlock() which is
// an inline; _or_ this function may have been called from
// Cyg_Scheduler::reschedule(), or Cyg_Scheduler::unlock_reschedule. The
// new_lock argument contains the value that the scheduler lock should
// have after this function has completed. If it is zero then the lock is
// being released and some extra work (running ASRs, checking for DSRs) is
// done before returning. If it is non-zero then it must equal the
// current value of the lock, and is used to indicate that we want to
// reacquire the scheduler lock before returning. This latter option
// only makes any sense if the current thread is no longer runnable,
// e.g. sleeping, otherwise this function will do nothing.
// This approach of passing in the lock value at the end effectively
// makes the scheduler lock a form of per-thread variable. Each call
// to unlock_inner() carries with it the value the scheduler should
// have when it reschedules this thread back, and leaves this function.
// When it is non-zero, and the thread is rescheduled, no ASRS are run,
// or DSRs processed. By doing this, it makes it possible for threads
// that want to go to sleep to wake up with the scheduler lock in the
// same state it was in before.

//unlock_inner
//鍙橀噺锛氭瘡涓嚎绋嬭嚜宸辩淮鎶ょ殑scheduler閿佺殑鍊�
//娑夊強鍒扮殑kernel鏁版嵁缁撴瀯锛歭ock锛宑urrent_thread
void Cyg_Scheduler::unlock_inner( cyg_ucount32 new_lock )
{
#ifdef CYGDBG_KERNEL_TRACE_UNLOCK_INNER
    CYG_REPORT_FUNCTION();
#endif    

    do {
    	//鏂伴攣鐨勫�鍙互鏄�
    	//0锛氬嵆褰诲簳閲婃斁閿�
    	//>=0锛氫笌褰撳墠閿佸�鐩稿悓锛屾垨鑰呮瘮褰撳墠閿佸�灏忎竴銆傚墠鑰呭搴旓紵鍚庤�瀵瑰簲浜庨噴鏀句竴閲嶉攣銆�
        CYG_PRECONDITION( new_lock==0 ? get_sched_lock() == 1 :
                          ((get_sched_lock() == new_lock) || (get_sched_lock() == new_lock+1)),
                          "sched_lock not at expected value" );
        
#ifdef CYGIMP_KERNEL_INTERRUPTS_DSRS
        
        // Call any pending DSRs. Do this here to ensure that any
        // threads that get awakened are properly scheduled.

        //鑻ラ攣瀹屽叏閲婃斁锛屽垯姝ゆ椂闇�灏唒endingDSR鍏ㄩ儴鍞ら啋鎵ц銆�
        if( new_lock == 0 && Cyg_Interrupt::DSRs_pending() )
            Cyg_Interrupt::call_pending_DSRs();
#endif
        //current锛氳〃绀哄綋鍓嶆鍦ㄨ繍琛岀殑绾跨▼锛屼篃灏辨槸璋冨害鎴栬�瑙﹀彂浜唘nlock_inner()鍑芥暟鐨勭嚎绋�
        Cyg_Thread *current = get_current_thread();
        //纭褰撳墠绾跨▼鏄ソ鐨勶紙浠�箞鏄ソ鐨勶紵锛燂紵锛燂級
        CYG_ASSERTCLASS( current, "Bad current thread" );

//姝ゅ涓ょ閰嶇疆锛屾垨鑰呮鏌ユ墍鏈夌殑绾跨▼鏍堬紝鎴栬�鍙鏌ョ嚎绋嬫爤
#ifdef CYGFUN_KERNEL_ALL_THREADS_STACK_CHECKING
        // should have  CYGVAR_KERNEL_THREADS_LIST
        //閬嶅巻褰撳墠绾跨▼鍒楄〃锛屾鏌ユ爤鏈夋病鏈夊嚭閿欙紙锛燂紵锛燂紵锛燂紵锛�
        //current琚綋鎴愪竴涓复鏃跺彉閲忔潵浣跨敤
        current = Cyg_Thread::get_list_head();
        while ( current ) {
            current->check_stack();
            current = current->get_list_next();
        }
        //current鍙堥噸鏂版仮澶嶄负鐪熸鐨勫綋鍓嶇嚎绋嬨�锛堣繖鏄负浜嗗噺灏戝唴瀛樼敵璇凤紵锛燂級
        current = get_current_thread();
#endif

#ifdef CYGFUN_KERNEL_THREADS_STACK_CHECKING
        current->check_stack();
#endif

        // If the current thread is going to sleep, or someone
        // wants a reschedule, choose another thread to run

        //涓や釜鏉′欢锛�
        //褰撳墠绾跨▼杩愯鐘舵�涓嶅啀鏄痳unning
        //杩欎釜unlock_inner()鏄敱鍏朵粬绾跨▼鎴朿pu寮曡捣鐨�
        if( current->state != Cyg_Thread::RUNNING || get_need_reschedule() ) {
        	//杩欏彧鏄竴鍙ユ帶鍒跺彴鎵撳嵃淇℃伅鐢ㄧ殑锛屼箣鍚庝笉鍐嶈В閲娿�
            CYG_INSTRUMENT_SCHED(RESCHEDULE,0,0);
            
            // Get the next thread to run from scheduler
            //scheduler杩斿洖涓�釜鎺ヤ笅鏉ラ�鍚堣繍琛岀殑绾跨▼銆�
            Cyg_Thread *next = scheduler.schedule();

            //妫�煡next鏄惁鏄釜鍚堟硶绾跨▼
            CYG_CHECK_DATA_PTR( next, "Invalid next thread pointer");
            CYG_ASSERTCLASS( next, "Bad next thread" );

            //鑻ext涓嶅啀鏄綋鍓峜urrent绾跨▼
            //锛堝疄闄呬笂鐢ㄤ簬姝nlock_inner()鐢卞叾浠栫嚎绋嬫垨鑰卌pu寮曡捣鐨勬儏鍐�
            //鍚﹀垯绾跨▼鏈潵宸茬粡鏃犳硶杩愯锛屼篃涓嶄細琚玸cheduler璋冨害涓簄ext锛�
            if( current != next )
            {

                CYG_INSTRUMENT_THREAD(SWITCH,current,next);

                // Count this thread switch
                //thread_switches璁板綍姣忎釜cpu鐨勪笂涓嬫枃鍒囨崲娆℃暟
                thread_switches[CYG_KERNEL_CPU_THIS()]++;

                //妫�煡鏍堢姸鍐�
#ifdef CYGFUN_KERNEL_THREADS_STACK_CHECKING
                next->check_stack(); // before running it
#endif

                //灏嗗綋鍓嶇嚎绋嬬殑杩愯鏃堕棿淇濆瓨
                //褰撹繑鍥炴椂浠嶆棫浠庤繖涓�椂闂村紑濮嬭繍琛�
                current->timeslice_save();
                
                // Switch contexts
                //HAL灞傝礋璐ｅ皢瀵勫瓨鍣ㄤ俊鎭帇鏍堝嚭鏍�
                HAL_THREAD_SWITCH_CONTEXT( &current->stack_ptr,
                                           &next->stack_ptr );

                // Worry here about possible compiler
                // optimizations across the above call that may try to
                // propogate common subexpresions.  We would end up
                // with the expression from one thread in its
                // successor. This is only a worry if we do not save
                // and restore the complete register set. We need a
                // way of marking functions that return into a
                // different context. A temporary fix would be to
                // disable CSE (-fdisable-cse) in the compiler.
                
                // We return here only when the current thread is
                // rescheduled.  There is a bit of housekeeping to do
                // here before we are allowed to go on our way.

                CYG_CHECK_DATA_PTR( current, "Invalid current thread pointer");
                CYG_ASSERTCLASS( current, "Bad current thread" );
                //current_thread鏁扮粍閲嶆柊璁板綍褰撳墠绾跨▼涓篶urrent銆�
                current_thread[CYG_KERNEL_CPU_THIS()] = current;   // restore current thread pointer
                //鎭㈠杩愯鏃堕棿鐗�
                current->timeslice_restore();
            }
            //rescheduling娓呴櫎浜嗐�
            //锛堜絾鏄痭eed_reschedule鏄瓨鍦ㄤ粈涔堝湴鏂圭殑浠�箞鍙橀噺鍟娿�銆傘�銆傘�銆傦級
            clear_need_reschedule();    // finished rescheduling
        }
        //鏂伴攣鐨勫�涓�
        if( new_lock == 0 )
        {
//ASR鏆傛椂鏃犳硶鎼炲畾銆傘�銆�
            
            //杩欎釜鍑芥暟涓嶇煡閬撴槸骞插槢鐨勩�
            HAL_REORDER_BARRIER(); // Make sure everything above has happened
                                   // by this point
            //閿佹竻闆�
            zero_sched_lock();     // Clear the lock
            HAL_REORDER_BARRIER();
                
#ifdef CYGIMP_KERNEL_INTERRUPTS_DSRS

            // Now check whether any DSRs got posted during the thread
            // switch and if so, go around again. Making this test after
            // the lock has been zeroed avoids a race condition in which
            // a DSR could have been posted during a reschedule, but would
            // not be run until the _next_ time we release the sched lock.

            //閿佸凡缁忔竻闆朵簡锛屾鏃惰閲嶆柊妫�煡DSR
            if( Cyg_Interrupt::DSRs_pending() ) {
                inc_sched_lock();   // reclaim the lock
                continue;           // go back to head of loop
            }

#endif
            // Otherwise the lock is zero, we can return.

//            CYG_POSTCONDITION( get_sched_lock() == 0, "sched_lock not zero" );



        }
        //閿佹病鏈夊畬鍏ㄩ噴鏀撅紝鍙槸鍑忎簡涓�噸
        else
        {
            // If new_lock is non-zero then we restore the sched_lock to
            // the value given.
            
            HAL_REORDER_BARRIER();
            //璁剧疆鏂伴攣鐨勫�銆�
            set_sched_lock(new_lock);
            
            HAL_REORDER_BARRIER();            
        }
        
#ifdef CYGDBG_KERNEL_TRACE_UNLOCK_INNER
        CYG_REPORT_RETURN();
#endif
        return;

    } while( 1 );

    CYG_FAIL( "Should not be executed" );
}

// -------------------------------------------------------------------------
// Thread startup. This is called from Cyg_Thread::thread_entry() and
// performs some housekeeping for a newly started thread.

void Cyg_Scheduler::thread_entry( Cyg_Thread *thread )
{
    clear_need_reschedule();            // finished rescheduling
    set_current_thread(thread);         // restore current thread pointer

    CYG_INSTRUMENT_THREAD(ENTER,thread,0);

    thread->timeslice_reset();
    thread->timeslice_restore();
    
    // Finally unlock the scheduler. As well as clearing the scheduler
    // lock this allows any pending DSRs to execute. The new thread
    // must start with a lock of zero, so we keep unlocking until the
    // lock reaches zero.
    while( get_sched_lock() != 0 )
        unlock();    
}

// -------------------------------------------------------------------------
// Start the scheduler. This is called after the initial threads have been
// created to start scheduling. It gets any other CPUs running, and then
// enters the scheduler.

void Cyg_Scheduler::start()
{
    CYG_REPORT_FUNCTION();
    
    start_cpu();
}

// -------------------------------------------------------------------------
// Start scheduling on this CPU. This is called on each CPU in the system
// when it is started.

void Cyg_Scheduler::start_cpu()
{
    CYG_REPORT_FUNCTION();
    
    // Get the first thread to run from scheduler
    register Cyg_Thread *next = scheduler.schedule();

    CYG_ASSERTCLASS( next, "Bad initial thread" );

    clear_need_reschedule();            // finished rescheduling
    set_current_thread(next);           // restore current thread pointer

#ifdef CYGVAR_KERNEL_COUNTERS_CLOCK
    // Reference the real time clock. This ensures that at least one
    // reference to the kernel_clock.o object exists, without which
    // the object will not be included while linking.
    CYG_REFERENCE_OBJECT( Cyg_Clock::real_time_clock );
#endif

    // Load the first thread. This will also enable interrupts since
    // the initial state of all threads is to have interrupts enabled.
    
    HAL_THREAD_LOAD_CONTEXT( &next->stack_ptr );    
    
}

// -------------------------------------------------------------------------
// SMP support functions

// -------------------------------------------------------------------------
// Consistency checker

#ifdef CYGDBG_USE_ASSERTS

cyg_bool Cyg_Scheduler::check_this( cyg_assert_class_zeal zeal) const
{
    CYG_REPORT_FUNCTION();
        
    // check that we have a non-NULL pointer first
    if( this == NULL ) return false;
    
    switch( zeal )
    {
    case cyg_system_test:
    case cyg_extreme:
    case cyg_thorough:
        if( !get_current_thread()->check_this(zeal) ) return false;
    case cyg_quick:
    case cyg_trivial:
    case cyg_none:
    default:
        break;
    };

    return true;
}

#endif

//==========================================================================
// SchedThread members

// -------------------------------------------------------------------------
// Static data members

// -------------------------------------------------------------------------
// Constructor

Cyg_SchedThread::Cyg_SchedThread(Cyg_Thread *thread, CYG_ADDRWORD sched_info)
: Cyg_SchedThread_Implementation(sched_info)
{
    CYG_REPORT_FUNCTION();
        
    queue = NULL;

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL

    mutex_count = 0;
    
#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_SIMPLE
    
    priority_inherited = false;
    
#endif
#endif

}

// -------------------------------------------------------------------------
// ASR support functions


// -------------------------------------------------------------------------
// Generic priority protocol support

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL

void Cyg_SchedThread::set_inherited_priority( cyg_priority pri, Cyg_Thread *thread )
{
    CYG_REPORT_FUNCTION();

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_SIMPLE

    // This is the comon code for priority inheritance and ceiling
    // protocols. This implementation provides a simplified version of
    // the protocol.
    
    Cyg_Thread *self = CYG_CLASSFROMBASE(Cyg_Thread,
                                         Cyg_SchedThread,
                                         this);

    CYG_ASSERT( mutex_count > 0, "Non-positive mutex count");
    
    // Compare with *current* priority in case thread has already
    // inherited - for relay case below.
    if( pri < priority )
    {
        cyg_priority mypri = priority;
        cyg_bool already_inherited = priority_inherited;

        // If this is first inheritance, copy the old pri
        // and set inherited flag. We clear it before setting the
        // pri since set_priority() is inheritance aware.
        // This is called with the sched locked, so no race conditions.

        priority_inherited = false;     // so that set_prio DTRT

        //self:褰撳墠绾跨▼
        //pri锛�
        self->set_priority( pri );            

        if( !already_inherited )
            original_priority = mypri;

        priority_inherited = true;      // regardless, because it is now

    }

#endif
}

void Cyg_SchedThread::relay_inherited_priority( Cyg_Thread *ex_owner, Cyg_ThreadQueue *pqueue)
{
    CYG_REPORT_FUNCTION();

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_SIMPLE

    // A simple implementation of priority inheritance.
    // At its simplest, this member does nothing.

    // If there is anyone else waiting, then the *new* owner inherits from
    // the current one, since that is a maxima of the others waiting.
    // (It's worth not doing if there's nobody waiting to prevent
    // unneccessary priority skew.)  This could be viewed as a discovered
    // priority ceiling.

    if ( !pqueue->empty() )
        set_inherited_priority( ex_owner->get_current_priority(), ex_owner );

#endif
}

void Cyg_SchedThread::clear_inherited_priority()
{
    CYG_REPORT_FUNCTION();

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_SIMPLE

    // A simple implementation of priority inheritance/ceiling
    // protocols.  The simplification in this algorithm is that we do
    // not reduce our priority until we have freed all mutexes
    // claimed. Hence we can continue to run at an artificially high
    // priority even when we should not.  However, since nested
    // mutexes are rare, the thread we have inherited from is likely
    // to be locking the same mutexes we are, and mutex claim periods
    // should be very short, the performance difference between this
    // and a more complex algorithm should be negligible. The most
    // important advantage of this algorithm is that it is fast and
    // deterministic.
    
    Cyg_Thread *self = CYG_CLASSFROMBASE(Cyg_Thread,
                                         Cyg_SchedThread,
                                         this);

    CYG_ASSERT( mutex_count >= 0, "Non-positive mutex count");
    
    if( mutex_count == 0 && priority_inherited )
    {
        priority_inherited = false;

        // Only make an effort if the priority must change
        if( priority < original_priority )
            self->set_priority( original_priority );
        
    }
    
#endif        
}

#endif // CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL

// -------------------------------------------------------------------------
// Priority inheritance support.

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_INHERIT

// -------------------------------------------------------------------------
// Inherit the priority of the provided thread if it
// has a higher priority than ours.

void Cyg_SchedThread::inherit_priority( Cyg_Thread *thread)
{
    CYG_REPORT_FUNCTION();

    Cyg_Thread *self = CYG_CLASSFROMBASE(Cyg_Thread,
                                         Cyg_SchedThread,
                                         this);

    CYG_ASSERT( mutex_count > 0, "Non-positive mutex count");
    CYG_ASSERT( self != thread, "Trying to inherit from self!");

    self->set_inherited_priority( thread->get_current_priority(), thread );
    
}

// -------------------------------------------------------------------------
// Inherit the priority of the ex-owner thread or from the queue if it
// has a higher priority than ours.

void Cyg_SchedThread::relay_priority( Cyg_Thread *ex_owner, Cyg_ThreadQueue *pqueue)
{
    CYG_REPORT_FUNCTION();

    relay_inherited_priority( ex_owner, pqueue );
}

// -------------------------------------------------------------------------
// Lose a priority inheritance

void Cyg_SchedThread::disinherit_priority()
{
    CYG_REPORT_FUNCTION();

    CYG_ASSERT( mutex_count >= 0, "Non-positive mutex count");

    clear_inherited_priority();
}

#endif // CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_INHERIT

// -------------------------------------------------------------------------
// Priority ceiling support

#ifdef CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_CEILING

void Cyg_SchedThread::set_priority_ceiling( cyg_priority pri )
{
    CYG_REPORT_FUNCTION();

    CYG_ASSERT( mutex_count > 0, "Non-positive mutex count");

    set_inherited_priority( pri );

}

void Cyg_SchedThread::clear_priority_ceiling( )
{
    CYG_REPORT_FUNCTION();

    CYG_ASSERT( mutex_count >= 0, "Non-positive mutex count");

    clear_inherited_priority();
}

#endif // CYGSEM_KERNEL_SYNCH_MUTEX_PRIORITY_INVERSION_PROTOCOL_CEILING

// -------------------------------------------------------------------------
// EOF sched/sched.cxx
