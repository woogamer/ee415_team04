#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "vm/Swap.h"
#include "vm/SPT.h"
#include "vm/FT.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"


/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static bool setup_page(struct intr_frame *, void * fault_addr);
static bool install_page (void *upage, void *kpage, bool writable);


/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      //thread_exit (); 
      sys_exit(-1);

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      //thread_exit ();
      sys_exit(-1);
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */

static bool
setup_page(struct intr_frame *f, void * fault_addr)
{
  uint8_t *kpage;
  bool success = false;
 // printf("setup_page %p\n", fault_addr);
  uint8_t *next_page=pg_round_down(fault_addr);
 // printf("setup_page\n");
  kpage = F_alloc (next_page,PAL_USER | PAL_ZERO);
  //kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) next_page), kpage, true);
      if (!success)
        palloc_free_page (kpage);
	else
	{
	// *(int *)(f->esp +4)=-1;
	//ASSERT(0);
	}
    }
  return success;
}
static bool 
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
 *      address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

static void
page_fault (struct intr_frame *f) 
{

struct thread *t = thread_current ();
//printf("\n\n\n\n\n\n\n\nPAGE FAULT START list_size = %d , tid = %d\n", list_size(&t->SPT), t->tid);
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

//printf("fault_addr=%p\n",fault_addr);
 

/* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;


t = thread_current();
//	printf("lock acquire START tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
	lock_acquire(&F_lock);
//	printf("lock acquire END tid = %d lock tid = %d\n", t->tid, F_lock.holder->tid);



//stack growth
if(fault_addr<PHYS_BASE && ((fault_addr==f->esp-32)|| (fault_addr == f->esp -4)))
{
	setup_page(f,fault_addr);
	lock_release(&F_lock);
//	printf("stack growth : %p\n",f->esp);
//	printf("%08x\n",*((int *)f->esp));
	return ;
}
//else
//printf("fault_addr<PHYS_BASE=%d && ((fault_addr==f->esp+32)=%d || (fault_addr == f->esp +4)=%d\n)",fault_addr<PHYS_BASE,fault_addr==f->esp+32, fault_addr == f->esp +4);

//printf("differ = %d\n",fault_addr-f->esp);
//

uint8_t *VMP=pg_round_down(fault_addr);
//printf("VVVVVVMP =%p\n",VMP);
//printf("FAULT_ADD =%p\n",fault_addr);

/*
struct list_elem *e;
      for (e = list_begin (&t->SPT); e != list_end (&t->SPT);
           e = list_next (e))
        {
          struct SPTE *temp = list_entry(e, struct SPTE, SPTE_elem);
          printf(" [elem = %p, status = %d] --", temp->VMP, temp->status);
        }
*/
//page_swaped
struct SPTE * spte = find_SPT(t, VMP);
//printf("spte = %p \n", spte);
if(spte &&spte->status==1)
{
	//printf("before evict list_size=%d tid=%d\n", list_size(&FT), t->tid);
	uint8_t *PMP = F_alloc(VMP, PAL_USER | PAL_ZERO);
	struct list_elem *e = (spte->SWTE_elem);
	struct SWTE * swte = list_entry(e, struct SWTE, SWTE_elem);
	//printf("evict list_size=%d tid=%d\n", list_size(&FT), t->tid);
/*
      for (e = list_begin (&SWT); e != list_end (&SWT);
           e = list_next (e))
        {
          struct SWTE *temp = list_entry(e, struct SWTE, SWTE_elem);
          printf(" [elem = %p] --", temp->VMP);
        }
*/
  	//intr_enable ();
	ASSERT(spte->status==1);
	disk_out(t, VMP, PMP);
	//pagedir update
	pagedir_set_page(t->pagedir, VMP, PMP, true);
	



        struct list_elem * elem = &swte->SWTE_elem;
	//printf("0\n");
	//printf("swte->SWTE_elem = %p\n", &swte->SWTE_elem);
	//printf("next = %p\n", &elem->next->prev);
	//printf("prev = %p\n", &elem->prev->next);
	//printf("head = %p\n", &SWT.head);
	//printf("tail = %p\n", &SWT.tail);
	//update SWT
	//disk_out already did
	//printf("1\n");
	
	//update FT
	struct FTE * fte = list_back(&FT);
	//update SPT
	
//	printf("2\n");
	spte->status=0;
	spte->FTE_elem = &fte->FTE_elem;
	spte->SWTE_elem =NULL;

	//printf("spte-> vmp = %p\n", spte->VMP);
/*
for (e = list_begin (&t->SPT); e != list_end (&t->SPT);
           e = list_next (e))
        {
          struct SPTE *temp = list_entry(e, struct SPTE, SPTE_elem);
          printf(" [elem = %p, status = %d] --", temp->VMP, temp->status);
        }
*/	
//	printf("3\n");
	
//	printf("lock release start tid = %d\n", t->tid);
	lock_release(&F_lock);

//	printf("lock release end tid = %d\n\n\n\n", t->tid);

//printf("PAGE FAULT END list_size = %d \n", list_size(&FT));
	return ;
}else if(spte &&spte->status==2){

	struct thread * curr = thread_current();	

	uint8_t * PMP = F_alloc(spte->VMP, PAL_USER | PAL_ZERO);
	pagedir_set_page(curr->pagedir, spte->VMP, PMP, true);
	
	file_seek(spte->file, spte->offset);
	file_read(spte->file, PMP, spte->endaddr - spte->VMP);
	

	spte->status = 3;
	lock_release(&F_lock);

	return;
}


if(fault_addr<PHYS_BASE && (fault_addr>=f->esp))
{
	setup_page(f,fault_addr);
	lock_release(&F_lock);
	//printf("stack growth : %p\n",f->esp);
	//printf("%08x\n",*((int *)f->esp));
	return ;
}
	
//	printf("lock release last start tid = %d\n", t->tid);
	lock_release(&F_lock);
//	printf("lock release last end tid = %d\n\n\n\n", t->tid);



/* When a page fault occurs, the process should terminate.  */
if(not_present || write || user)
{
//	printf("not_present = %d, write =%d, user=%d, fault addr=%p tid = %d\n", not_present, write, user, fault_addr, t->tid);
	//printf("123");
	sys_exit(-1);

  /* Never reached */

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");


  kill (f);
}
}
