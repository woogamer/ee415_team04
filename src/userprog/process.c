#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "vm/FT.h"
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);

  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* thread name size <= 16
   * therefore when thread_create call, the name of thread should not contain arguments */
  char temp[16];
  int i=0;
  while(i<15)
  {
	temp[i]= file_name[i];
	if(temp[i]==' ')
	temp[i]='\0';
	i++;
  }
  temp[15]='\0';	  

  /* The purpose of the sema_char structure below is */ 
  /*  to check success of failure of loading execution file by child.  */
  struct sema_char *s_c = (struct sema_char *)malloc(sizeof(struct sema_char));
  sema_init(&s_c->sema, 0);
  s_c->file_name = fn_copy;

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (temp, PRI_DEFAULT, start_process, s_c);

  //printf("************************ thread create\n");

  /* When it fails because of some fails(e.g., fail to obtain a free page), it returns -1.*/
  if (tid == TID_ERROR){
	  free(s_c);
      palloc_free_page (fn_copy); 
	  return tid;
  }
 
  /* To check whether the child process loads the execution file successfully or not. */
  sema_down(&s_c->sema);

  /* If the child process fails to load, it returns -1. */
  if(!s_c->success){
	tid = TID_ERROR;
  }

  free(s_c);
  return tid;
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *f_name)
{
  struct sema_char *s_c = (struct sema_char *)f_name;
  char *file_name = s_c->file_name;

  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  //printf("************************ load start\n");
  success = load (file_name, &if_.eip, &if_.esp);

  //printf("************************ load end\n");
  s_c->success = success;

  palloc_free_page (file_name);
  sema_up(&s_c->sema);

  /* If the load failed, it quits this process. */
  if (!success){
    sys_exit(-1);
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
	int status;
	int realchild = 0;
  	struct thread * curr = thread_current();
	struct thread * child = NULL;
	struct list_elem *find;

	if(child_tid == TID_ERROR)
		return -1;

	enum intr_level old_level;
	old_level = intr_disable ();

	/* If child_tid has already been terminated, wait() returns its exit status. */
	if(!list_empty(&curr->terminated_child_list)){
		for(find = list_begin(&curr->terminated_child_list);
			find != list_end(&curr->terminated_child_list);
			find = list_next(find))
		{
			struct terminated_proc_info *temp = list_entry(find, struct terminated_proc_info, terminated_elem);
			if(temp->tid == child_tid){
				list_remove(find);
				status = temp->exit_status;

				/* If a parent process waits a same child twice, it should return -1. */
				/* To do this, it frees the temp information. */
				free(temp);
				return status;
			}
		}
	}

	/* Check whether child_tid is in the child_list of the running thread or not */
	if(!list_empty(&curr->child_list)){
		for(find = list_begin(&curr->child_list);
			find != list_end(&curr->child_list);
			find = list_next(find))
		{
			child = list_entry(find, struct thread, child_elem);
			if(child->tid == child_tid){
				realchild = 1;
				break;
			}
		}
	}
	intr_set_level (old_level);

	/* If child_tid does not exist in the child list, then*/
	if(!realchild)
	{
		/* return -1 */
		return -1;
	}else{
		sema_down(&child->exit_sema);
		/* When it reaches here, the child has called exit() by itself. */

		/* When the child calls exit(), the elem of the child is moved to the terminated list of the parent. */
		old_level = intr_disable ();
		for(find = list_begin(&curr->terminated_child_list);
            find != list_end(&curr->terminated_child_list);
            find = list_next(find))
        {
            struct terminated_proc_info *temp = list_entry(find, struct terminated_proc_info, terminated_elem);
            if(temp->tid == child_tid){
				list_remove(find);
				status = temp->exit_status;
				free(temp);
                return status;
            }
        }
		intr_set_level (old_level);
	}

	/* Never reached */
	NOT_REACHED ();
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *curr = thread_current ();
  uint32_t *pd;


  printf("\n\n\nzzzzzzzzzzzzzzzzzzzzzzzzEXIT tid=%d holder=%d\n",curr->tid, F_lock.holder->tid);
  if(!lock_held_by_current_thread(&F_lock))  
  {
	printf("sadfaafds");
	lock_acquire(&F_lock);
  }
  printf("222222");

  printf("\n\n\nzzzzzzzzzzzzzzzzzzzzzzzzsys EXIT tid=%d holder=%d\n",curr->tid, F_lock.holder->tid);
  if(!lock_held_by_current_thread(&sys_lock))  
  {
	printf("sadfaafds");
	lock_acquire(&sys_lock);
  }
  printf("333333");


  delete_FT();
printf("1\n");
  delete_SWT();

printf("2\n");
  delete_SPT();

  printf("11111111111f");
  lock_release(&F_lock);
  printf("\n\n\nzzzzzzzzzzzzzzzzzzzzzzzzEXIT tid=%d\n\n\n",curr->tid);
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = curr->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      curr->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();


  /* Before file_open, find function name*/
  char temp[16];
  char* save_ptr;
  i=0;
  while(i<15)
  {
	temp[i]= file_name[i];
	if(temp[i]==' ')
	temp[i]='\0';
	i++;
  }
  temp[15]='\0';	  


  printf("************************ file open tid=%d\n",t->tid);
  /* Open executable file. */
  file = filesys_open (temp);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  file_deny_write(file);
  t->exec_file = file;
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;
printf("1");
      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

printf("2");
      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {

printf("3");
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);

printf("4");
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);

printf("5");
                }

printf("6\n");
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  printf("************************ setup stack start tid=%d\n",t->tid);
  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  printf("************************ setup stack end tid=%d\n",t->tid);
  /* After stack setup make argument passing*/

  /* --------------- parsing argument --------------*/
  // string that result of strtok_r()
  char *token;
  // string size of token
  int size_token;
  // the number of arguments
  int num_token=0;
  i=0;
  for (token = strtok_r (file_name, " ", &save_ptr); token != NULL;
        token = strtok_r (NULL, " ", &save_ptr))
  {
	
	size_token= strlen(token)+1;
	// whenever esp is updated, check that it is in one page
  	ASSERT((PHYS_BASE-*esp-size_token)<4000);
	*esp = *esp - size_token;
	// push data in stack
	memcpy(*esp, token, size_token);
	//increase counter when argument is parsed
	num_token++;

  }   
  // ---------------word align------------------
  size_t align =(size_t) *esp%4;
  char *zero=*esp;
  for(i=0; (size_t)i<align; i++)
  {
	zero= zero-(size_t)1;
	
  	ASSERT((PHYS_BASE-*esp-4)<4000);
	*esp= *esp-(size_t)1; 
	*zero=0;
  }
  ASSERT((size_t)*esp%4==0);
  //--------------------------------------------

  //zero padding
  zero=*esp;
  for(i=0; i<4; i++)
  {
	zero= zero-(size_t)1;

  	ASSERT((PHYS_BASE-*esp-4)<4000);
	*esp= *esp-(size_t)1; 
	*zero=0;
  }


 // push pointers that points address of arguments
 
 // find is pointer that find the address of argumets it goes up from esp   
 char * find= *esp;
 // address of arguments
 int address;
  i=num_token;
  
  while(true)
  {
	//if it find not null character, find is the address of first argument
	if(*find!='\0')
	break;
	find++;
  }
  while(true)
  {
	//push the address of arguments
  	ASSERT((PHYS_BASE-*esp-4)<4000);
	*esp = *esp -(size_t) 4;
        address=(int)find;
	memcpy(*esp, &address, 4);
	// if it find all arguments then break
	if(--i==0)
	break;
	while(true)
	{
		
		//if it find null character, the address of next argument is find+1 
		if(*find=='\0')
		break;
		find++;
	}
	find++;
  }
  //push pointer that points argv[]
  int argv_start=(int)*esp;
  
  ASSERT((PHYS_BASE-*esp-4)<4000);
  *esp = *esp -(size_t) 4;
  memcpy(*esp, &argv_start, 4);

 //push the number of arguments
  ASSERT((PHYS_BASE-*esp-4)<4000);
  *esp = *esp -(size_t) 4;
  memcpy(*esp,(char*) &num_token, 4);
  //push retrun address
  zero=*esp;
  for(i=0; i<4; i++)
  {
	zero= zero-(size_t)1;
  	ASSERT((PHYS_BASE-*esp-4)<4000);
	*esp= *esp-(size_t)1; 
	*zero=0;
  }
  
  	
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  //file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* /p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

printf("7");
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      //uint8_t *kpage = palloc_get_page (PAL_USER);
     

	struct thread *	t = thread_current();
	if(F_lock.holder!=NULL)
	printf("load segment F_lock  START tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
	else
	printf("holder is NULL\n\n\n\n\n");
	lock_acquire(&F_lock);


	if(F_lock.holder!=NULL)
	printf("load segment F_lock END tid = %d lock tid = %d\n", t->tid, F_lock.holder->tid);
	else
	printf("holder is NULL\n\n\n\n\n");
	


     // printf("load segment F_lock START curr =%d holder=%d\n", thread_current()->tid, F_lock.holder->tid);
      //lock_acquire(&F_lock);
      //printf("load segment F_lock END curr =%d holder=%d\n", thread_current()->tid, F_lock.holder->tid);
printf("8");
      uint8_t *kpage = F_alloc (upage, PAL_USER);

printf("9");
	
	if(F_lock.holder!=NULL)
	printf("load segment release  START tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
	lock_release(&F_lock);

	if(F_lock.holder!=NULL)
	printf("load segment release END tid = %d lock tid = %d\n", t->tid, F_lock.holder->tid);


printf("aaa");
      if (kpage == NULL)
	{
	
	lock_release(&F_lock);
        return false;
	}
 	//printf("[load 1]");
      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
	 
	  lock_release(&F_lock);
          return false; 
        }

 	//printf("[load ]2");
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
	  lock_release(&F_lock);
          return false; 
        }

 	//printf("[load 3]");
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;
  //printf("page fault setup_stack\n");
  
  struct thread * t = thread_current();
  if(F_lock.holder!=NULL)
  printf("setup stack aquire  START tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
  lock_acquire(&F_lock);
  if(F_lock.holder!=NULL)
  printf("setup stack aquire  END tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
 
  kpage=F_alloc(((uint8_t *) PHYS_BASE) - PGSIZE, PAL_USER | PAL_ZERO);

  //kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

  if(F_lock.holder!=NULL)
  printf("setup stack release  START tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
  lock_release(&F_lock);
  if(F_lock.holder!=NULL)
  printf("setup stack release  END tid = %d lock tid =%d\n", t->tid, F_lock.holder->tid);
 

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void sys_exit(int status){
	struct thread *curr = thread_current();
	struct thread *parent = curr->parent;

	enum intr_level old_level = intr_disable ();

	list_remove(&curr->child_elem);

	/* Leave the small information for the terminated process  */
	struct terminated_proc_info *terminated_info = malloc(sizeof(struct terminated_proc_info));
	terminated_info->exit_status = status; /* exit status */
	terminated_info->tid = curr->tid;      /* tid of the terminated process */

	/* Keep the information into the term_list of the parent process */
	list_push_back(&parent->terminated_child_list, &terminated_info->terminated_elem);	

	/* Remove all file opened by the process */
	struct list_elem *find;
	if(!list_empty(&curr->fd_list)){
		for(find = list_begin(&curr->fd_list);	
			find != list_end(&curr->fd_list);
			find = list_next(find))
		{
			struct file *temp = list_entry(find, struct file, elem);
			find = list_remove(find);
			find = list_prev(find);
			file_close(temp);
		}
	}

	/* Free all the information of the terminated childs */
	if(!list_empty(&curr->terminated_child_list)){
		for(find = list_begin(&curr->terminated_child_list);	
			find != list_end(&curr->terminated_child_list);
			find = list_next(find))
		{
			struct terminated_proc_info *temp = list_entry(find, struct terminated_proc_info, terminated_elem);
			find = list_remove(find);
			find = list_prev(find);
			free(temp);
		}
	}

	printf("%s(%d): exit(%d)\n",  curr->name,curr->tid, status);
	//printf("%s: exit(%d)\n",  curr->name, status);
	
	intr_set_level (old_level);

	/* The parent process could wait for the child to be terminated by trying to down a semaphore.  */
	/* When a process terminates, it should up the semaphore to free the waiting parent. */
	sema_up(&curr->exit_sema);

	/* And also, it should close the execution file. (binary file)  */
	file_close(curr->exec_file);

	/* Finally, it changes the process(thread) status and destroy the page. */
	thread_exit();
}
