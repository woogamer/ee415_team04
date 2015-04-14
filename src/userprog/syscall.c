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

	int fd;
	char *buffer;
	int length;
	struct list_elem *find;
	struct file *file;
	unsigned position;

  //printf("Systellcall handler called!: %d\n",*(int *)f->esp);

  int sys_num = *(int *)(f->esp);
  switch(sys_num)
  {
	// pid_t exec (const char *file)
	case SYS_EXEC:
		isUseraddr(f->esp,1);
		if(*(char **)(f->esp + 4) == NULL){
			/* TODO: fill something here. */
		}
		f->eax = process_execute( *(char **)(f->esp + 4));
		break;
	
	//bool create (const char *file, unsigned initial_size)
	case SYS_CREATE:
		isUseraddr(f->esp,2);
		f->eax = filesys_create( *(char **)(f->esp+4), *(off_t *)(f->esp+8));
		break;

	// void exit (int status)
	case SYS_EXIT:
		isUseraddr(f->esp,1);
		sys_exit(*(int *)(f->esp+4));
		break;

	//int read (int fd, void *buffer, unsigned size)
	case SYS_READ:
		isUseraddr(f->esp,3);
  		lock_acquire(&sys_lock);

		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);

		/* read from the keyboard*/
		if(fd == 0){
			/* TODO: fill something here. */
			//f->eax = input_getc();
		}else{
			for(find = list_begin(&curr->fd_list);
			find != list_end(&curr->fd_list);
			find = list_next(find))
			{
				file = list_entry(find, struct file, elem);
				if(file->fd == fd)
					break;
			}
			if(file == NULL){
				/* TODO: fill something here. */
				lock_release(&sys_lock);
				break;
			}
			
			f->eax = file_read(file, buffer, length);
		}
		
		lock_release(&sys_lock);
		break;

	//int filesize (int fd)
	case SYS_FILESIZE:
		isUseraddr(f->esp,1);
  		lock_acquire(&sys_lock);
		fd = *(int *)(f->esp + 4);

		for(find = list_begin(&curr->fd_list);
		find != list_end(&curr->fd_list);
		find = list_next(find))
		{
			file = list_entry(find, struct file, elem);
			if(file->fd == fd)
				break;
		}
		if(file == NULL){
			/* TODO: fill something here. */
			lock_release(&sys_lock);
			break;
		}
		
		f->eax = file_length(file);
	
		lock_release(&sys_lock);
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
  
  		char * name = *(char **)(f->esp+4);
		file = filesys_open(name);
		/* null pointer*/
		if(file == NULL)
			f->eax = -1;
		else
		{
			f->eax = curr->fd_num;
			file->fd= curr->fd_num;
			curr->fd_num++;
			list_push_back(&curr->fd_list, &file->elem);
		}
		lock_release(&sys_lock);
		break;
		
  
  // case 9
  // int write (int fd, const void *buffer, unsigned length);
	case SYS_WRITE:
		isUseraddr(f->esp,3);
		lock_acquire(&sys_lock);

		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);

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
			file = fd2file(fd);

			if(!file)
				f->eax = -1;
			else{
				f->eax = file_write(file, buffer, length);		
			}
		}
		lock_release(&sys_lock);
		break;

	//void seek (int fd, unsigned position)
	case SYS_SEEK:
		isUseraddr(f->esp,2);
		fd = *(int*)(f->esp+4);
		position = *(unsigned*)(f->esp+8);

		file = fd2file(fd);
		file_seek(file, (off_t)position);
		
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
