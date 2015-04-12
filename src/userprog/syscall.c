#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
struct thread * curr;
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  struct lock sys_lock;
  lock_init(&sys_lock);
  curr = thread_current();
  struct thread * parent;
  //printf("Systellcall handler called!: %d\n",*(int *)f->esp);

  int sys_num = *(int *)(f->esp);
  switch(sys_num)
  {
	case SYS_EXIT:
	parent = thread_current()->parent; 
	struct list_elem *find;
	    
	for(find = list_begin(&parent->child_list);
	find != list_end(&parent->child_list);
	find = list_next(find))
	{
	    if(find==&thread_current()->child_elem)
	    {
	   	enum intr_level old_level;
	    	old_level = intr_disable ();
	    	list_remove(find);
	    	intr_set_level (old_level);
	    }
	}
	printf("%s: exit(%d)\n", thread_current()->name,*(int *)(f->esp+4));
	thread_exit();
	break;


  // case 3
  // int wait (tid_t tid)
	case SYS_WAIT:
	isUseraddr(f->esp,1);
	tid_t pid = *(tid_t*)(f->esp+4);
	f->eax = process_wait(pid);
	break;
  // case 6
  // int open (const char * file)
  	case SYS_OPEN:
  
  		isUseraddr(f->esp,1);
  		lock_acquire(&sys_lock);
  
  		char * name;
  		name= *(char **)(f->esp+4);
		struct file * file=filesys_open(name);
		/* null pointer*/
		if(!file)
		f->eax = -1;
		else
		{
	
			f->eax = curr->fd_num;
			file->fd= curr->fd_num;
			curr->fd_num++;
			list_push_back(&curr->fd_list, &file->elem);
		}
		break;
		
  
  // case 9
  // int write (int fd, const void *buffer, unsigned length);
  case SYS_WRITE:

  isUseraddr(f->esp,3);
  lock_acquire(&sys_lock);

  int fd = *(int*)(f->esp+4);
  char *buffer = *(char**)(f->esp+8);	
  int length = *(int*)(f->esp+12);

  /*stdin case*/  
  if(fd == 0)
  f->eax=-1;
  
  /*stdout case*/
  else if(fd==1)
  {
  	putbuf(buffer, length);
  	f->eax=length;
  }
 
  else
  {
	struct file * file = fd2file(fd);
	
  	if(!file)
	f->eax = -1;
	else
	f->eax = file_write(file, buffer, length);		
  }
  lock_release(&sys_lock);
  break;


   }
}



void
isUseraddr(void *esp, int argnum)
{
	if((uint32_t)esp+argnum*4+4 > (uint32_t) PHYS_BASE)
	//syscall_exit(-1);
	;
}
struct file *
fd2file(int fd)
{
	struct list_elem *find;
        for(find = list_begin(&curr->fd_list);
            find != list_end(&curr->fd_list);
            find = list_next(find))
            {
		struct file * f = list_entry(find, struct file, elem);
		
		if(fd== f->fd)
		return f;
	

	    }
	//can not find fd
	return 0;
}
