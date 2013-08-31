Set Implicit Arguments.

Add LoadPath "../HAL".
Require Import Environment.
Require Import NPeano.

Definition thread_entry := nat.

Definition CYGNUM_KERNEL_THREADS_STACK_CHECK_DATA_SIZE := 0.

(*  We implement a thread stack based on list of nat. 
 *  stack_base points to the first element of the list. 
 *  stack_ptr points to the last element of the list.
 *  stack_size would be the length of the list.
 *  saved_context is the HAL_SavedRegisters, which is in stack originally, but took
 *  out and stored separately.
 *  By "points to", I mean "be the index of"...
 *)
Section ThreadRegisterSec. 

  Record ThreadRegisters := mkTR {
    core : CoreRegister;
    basepri : nat
  (*LR is not included, so r14 should be 0*)
  }.

  Definition get_core_register_tr tr n := Environment.get_core_register tr.(core) n.

  Definition set_core_register_tr tr n v : ThreadRegisters := 
    mkTR (Environment.set_core_register tr.(core) n v) tr.(basepri).

  Definition set_basepri_tr tr n := mkTR tr.(core) n.

End ThreadRegisterSec.

Record HardwareThread := mkHT {
  stack_base : nat;
  stack_size : nat;

  stack_limit : nat;

  stack_ptr : nat;
  
  entry_point : thread_entry; (*pointer to the function*)

  entry_data : nat; (*pointer to the data*)

  (* There shouldn't be a thread without a context *)
  saved_context : ThreadRegisters; 
  
  stack : list nat
}.

Definition get_stack_base ht := ht.(stack_base) - CYGNUM_KERNEL_THREADS_STACK_CHECK_DATA_SIZE.

Definition set_stack_base ht sb := 
  mkHT sb ht.(stack_size) ht.(stack_limit) ht.(stack_ptr) ht.(entry_point) 
       ht.(entry_data) ht.(saved_context) ht.(stack).

Definition get_stack_size ht := ht.(stack_size) + 2 * CYGNUM_KERNEL_THREADS_STACK_CHECK_DATA_SIZE.

Definition set_stack_size ht ss :=
  mkHT ht.(stack_base) ss ht.(stack_limit) ht.(stack_ptr) ht.(entry_point) 
       ht.(entry_data) ht.(saved_context) ht.(stack).

Definition get_stack_limit ht := ht.(stack_limit).

Definition set_stack_limit ht sl :=
  mkHT ht.(stack_base) ht.(stack_size) sl ht.(stack_ptr) ht.(entry_point) 
       ht.(entry_data) ht.(saved_context) ht.(stack).

Definition get_stack_ptr ht := ht.(stack_ptr).

Definition set_stack_ptr ht sp :=
  mkHT ht.(stack_base) ht.(stack_size) ht.(stack_limit) sp ht.(entry_point) 
       ht.(entry_data) ht.(saved_context) ht.(stack).

Definition get_entry_point ht := ht.(entry_point).

Definition set_entry_point ht ep := 
  mkHT ht.(stack_base) ht.(stack_size) ht.(stack_limit) ht.(stack_ptr) ep 
       ht.(entry_data) ht.(saved_context) ht.(stack).

Definition get_entry_data ht := ht.(entry_data).

Definition set_entry_data ht ed := 
  mkHT ht.(stack_base) ht.(stack_size) ht.(stack_limit) ht.(stack_ptr) ht.(entry_point) 
       ed ht.(saved_context) ht.(stack).

Definition get_saved_context ht := ht.(saved_context).

Definition set_saved_context ht regs := 
  mkHT ht.(stack_base) ht.(stack_size) ht.(stack_limit) ht.(stack_ptr) ht.(entry_point) 
       ht.(entry_data) regs ht.(stack).

Definition get_stack ht := ht.(stack).

Definition set_stack ht st :=  
  mkHT ht.(stack_base) ht.(stack_size) ht.(stack_limit) ht.(stack_ptr) ht.(entry_point) 
       ht.(entry_data) ht.(saved_context) st.

Definition get_core_register ht n := get_core_register_tr ht.(saved_context) n.   

Definition set_core_register ht n v : HardwareThread := 
  set_saved_context ht (set_core_register_tr ht.(saved_context) n v).

Definition get_basepri ht := ht.(saved_context).(basepri).  

Definition set_basepri ht n := 
  set_saved_context ht (set_basepri_tr ht.(saved_context) n).

(*We don't use pointer here so only unique_id will suffice*)
Definition init_context ht uid : HardwareThread :=
  mkHT ht.(stack_base) ht.(stack_size) ht.(stack_limit) ht.(stack_ptr) ht.(entry_point) 
       ht.(entry_data) (mkTR (mkCR uid 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ht.(entry_point)) 0)
       (cons ht.(entry_point) nil).

(*TODO: switch_context (hs : HardState)(ht1 ht2 : HardwareThread) : HardState * HardwareThread.
*)

Definition attach_stack ht s_base s_size :=
  match ht with
  |mkHT _ _ _ _ ep ed sc st => mkHT s_base s_size s_base (s_base + s_size) ep ed sc st
  end. 

Definition HardwareThread_cstr e_point e_data s_size s_base :=
  attach_stack (mkHT 0 0 0 0 e_point e_data 
                     (mkTR (mkCR 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) 0) nil) 
               s_base s_size.

(*TODO: detach_stack, no definition found*)

(*TODO: prepare_exception*)

(*TODO: thread_entry*)