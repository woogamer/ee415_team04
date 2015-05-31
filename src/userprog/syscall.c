#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "threads/init.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "vm/FT.h"
#include "vm/SPT.h"
#include "vm/Swap.h"


static void syscall_handler (struct intr_frame *);
//void munmap_clear(struct MMTE *m);
struct thread * curr;



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&sys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  curr = thread_current();

  /* Variables uesd by system calls of several kinds*/
	int fd;
	char *buffer;
	int length;
	struct list_elem *find;
	struct file *file;
	unsigned position;
	int mmap_id;

  /* Check the address of esp */
  isUseraddr(f->esp,0,0);
  if(!pagedir_get_page(curr->pagedir, f->esp))
	  sys_exit(-1);

  int sys_num = *(int *)(f->esp);
  switch(sys_num)
  {
	// pid_t exec (const char *file)
	case SYS_EXEC:
		isUseraddr(f->esp,1,1);
		if(*(char **)(f->esp + 4) == NULL){
			f->eax = TID_ERROR;
			break;
		}

  		//lock_acquire(&sys_lock);
 		f->eax = process_execute( *(char **)(f->esp + 4));
  		//lock_release(&sys_lock);
		break;
	
	//bool create (const char *file, unsigned initial_size)
	case SYS_CREATE:
		isUseraddr(f->esp,2,1);
		
  		lock_acquire(&sys_lock);
		f->eax = filesys_create( *(char **)(f->esp+4), *(off_t *)(f->esp+8));
  		lock_release(&sys_lock);
		break;

	// void exit (int status)
	case SYS_EXIT:
		isUseraddr(f->esp,1,0);
		sys_exit(*(int *)(f->esp+4));
		break;

	//int read (int fd, void *buffer, unsigned size)
	case SYS_READ:
		isUseraddr(f->esp,3,2);
  		lock_acquire(&sys_lock);

		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);
		
		/* stdin case*/
		if(fd == 0){
			int i = 0;

			uint8_t returnc;
			/* read untill null character appears or length is done*/
			while(i < length){
				returnc = input_getc();
				if(returnc=='\n')
					break;
				buffer[i++]=returnc;
			}
			f->eax = i;

		}
		/* stdout in case*/
		else if(fd ==1)
			f->eax = -1;
		/* normal case read from file*/
		else{
			file = NULL;
			bool suc=false;
			for(find = list_begin(&curr->fd_list);
			find != list_end(&curr->fd_list);
			find = list_next(find))
			{
				file = list_entry(find, struct file, elem);
				if(file->fd == fd)
				{
					suc=true;
					break;
				}
				
			}
			if(suc==false){
				lock_release(&sys_lock);
				f->eax = -1;
				return;
			}
			
			f->eax = file_read(file, buffer, length);
		}
		
		lock_release(&sys_lock);
		break;

	//int filesize (int fd)
	case SYS_FILESIZE:
		isUseraddr(f->esp,1,0);
		fd = *(int*)(f->esp+4);

		lock_acquire(&sys_lock);
		f->eax =sys_file_size(fd);
		lock_release(&sys_lock);
  		break;

    // int wait (tid_t tid)
	case SYS_WAIT:
		isUseraddr(f->esp,1,0);
		tid_t pid = *(tid_t*)(f->esp+4);
		f->eax = process_wait(pid);
		break;

    // int open (const char * file)
  	case SYS_OPEN:
  		isUseraddr(f->esp,1,1);
  		lock_acquire(&sys_lock);
  

  		char * name = *(char **)(f->esp+4);
		file = filesys_open(name);
		/* null pointer*/
		if(file == NULL)
			f->eax = -1;
		else
		{
			/* assgin fd_num*/
			f->eax = curr->fd_num;
			file->fd= curr->fd_num;
			curr->fd_num++;

			/* push fd information to thread*/
			list_push_back(&curr->fd_list, &file->elem);
		}
		lock_release(&sys_lock);
		break;
		
  
    // int write (int fd, const void *buffer, unsigned length);
	case SYS_WRITE:
		isUseraddr(f->esp,3,2);

		//printf("lock acquire start curr: %s, holder: %s\n", curr->name, sys_lock.holder->name);
		lock_acquire(&sys_lock);
		//printf("lock acquire end curr: %s, holder: %s\n", curr->name, sys_lock.holder->name);

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
		/*normal case write from file*/
		else
		{
			file = fd2file(fd);

			if(!file)
				f->eax = -1;
			else{
				f->eax = file_write(file, buffer, length);		
			}
		}
		//printf("lock release start curr: %s, holder: %s\n", curr->name, sys_lock.holder->name);
		lock_release(&sys_lock);
		//printf("lock release end curr: %s, holder: %s\n", curr->name, sys_lock.holder->name);
		break;

	//void seek (int fd, unsigned position)
	case SYS_SEEK:
		isUseraddr(f->esp,2,0);
		fd = *(int*)(f->esp+4);
		position = *(unsigned*)(f->esp+8);

		file = fd2file(fd);

		lock_acquire(&sys_lock);
		file_seek(file, (off_t)position);
		lock_release(&sys_lock);
		
		break;

	// void close (int fd)
	case SYS_CLOSE:
		isUseraddr(f->esp,1,0);
		fd = *(int*)(f->esp+4);

		file = fd2file(fd);

		/* Remove the elem of the file in the fd_list of the thread */
		enum intr_level old_level = intr_disable ();
		list_remove(&file->elem);
		intr_set_level (old_level);

		/* Close the file */
		lock_acquire(&sys_lock);
		file_close(file);
		lock_release(&sys_lock);
		break;

	case SYS_TELL:
		isUseraddr(f->esp,1,0);
		fd = *(int*)(f->esp+4);

		file = fd2file(fd);

		lock_acquire(&sys_lock);
		f->eax = file_tell(file);
		lock_release(&sys_lock);
		break;

	case SYS_HALT:
		power_off();
		break;

	case SYS_REMOVE:
		buffer = *(char **)(f->esp + 4);

		lock_acquire(&sys_lock);
		f->eax = filesys_remove(buffer);
		lock_release(&sys_lock);
		
		break;

    	case SYS_MMAP:
		isUseraddr(f->esp,2,0);
		fd = *(int*)(f->esp+4);
		position = *(unsigned*)(f->esp+8);
		file = fd2file(fd);
		int length = sys_file_size(fd);
		int pages;

		if(file==NULL|| position==0 || position%PGSIZE!=0)
		{
			f->eax=-1;
			break;
		}
		//	printf("hahah\n");
		
		pages = length/PGSIZE;
		if(length%PGSIZE)
		pages++;
		int temp_pages=pages;
		uint8_t * temp_position = position;
		struct SPTE *spte;
		//printf("length=%d pages=%d\n",length,pages);	
		while(temp_pages>0)
		{
		 spte = find_SPT(curr, temp_position);
		 //printf("VMP = %p\n",temp_position);
			if(spte!=NULL)
			{
		//		printf("Execption");
				f->eax=-1;
				return;
			}
			temp_pages--;
			temp_position=temp_position+PGSIZE;
		}

		lock_acquire(&sys_lock);
		file=file_reopen(file);	
		lock_release(&sys_lock);
		
		temp_pages=pages;
		temp_position=position;
		int offset=0;
		while(temp_pages>0)
		{
			struct SPTE * spte = malloc(sizeof(struct SPTE));
			spte->status=2;//mmap not load
			spte->VMP=temp_position;
			if(temp_pages==1)
			spte->endaddr=position+length;
			else
			spte->endaddr=spte->VMP+PGSIZE;

			spte->offset=offset;

			spte->file=file;
			list_push_back(&curr->SPT,&spte->SPTE_elem);
			
			offset=offset+PGSIZE;
			temp_position=temp_position+PGSIZE;	
			temp_pages--;

		}

		struct MMTE * mmte = malloc(sizeof(struct MMTE));
		mmte->id = curr->mmap_id;
		mmte->file = file;
		mmte->length= length;
		mmte->VMP=position;
		list_push_back(&curr->MMT, &mmte->MMTE_elem);
		
		f->eax= mmte->id;
		curr->mmap_id++;
		break;
	
    	case SYS_MUNMAP:
		mmap_id = *(int *)(f->esp+4);

		struct list_elem *e;
		struct MMTE * m;

		for(e = list_begin(&curr->MMT); e != list_end(&curr->MMT); ) {
			m = list_entry(e, struct MMTE, MMTE_elem);
			if(m->id == mmap_id) {
				munmap_clear(m);
				e = list_remove(e);

				lock_acquire(&sys_lock);
				file_close(m->file);
				lock_release(&sys_lock);

				free(m);
				return;
			}else{
				e = list_next(e);
			}
		}
		break;

   }
}


//check that the address is user address and check it is valid address
void
isUseraddr(void *esp, int argnum, int pointer_index)
{
	//check it is user address 
	if((uint32_t)esp + 4 + argnum*4 > (uint32_t) PHYS_BASE)
	sys_exit(-1);
	//check the pointer address is valid
	if(pointer_index > 0)
	{
		char **temp= (char**)(esp+pointer_index*4);

		if((uint32_t)*temp >= (uint32_t) PHYS_BASE)
			sys_exit(-1);
		// char pointer validation check
		if(!pagedir_get_page(curr->pagedir, *temp))
		{
			if(*temp>PHYS_BASE || *temp<=0)
        		sys_exit(-1);
		}
	}
	
}
//change fd to file pointer
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


int sys_file_size(int fd)
{
		int result;
		struct list_elem *find;
		struct file *file=NULL;
		/* find file from fd_list*/	
		for(find = list_begin(&curr->fd_list);
		find != list_end(&curr->fd_list);
		find = list_next(find))
		{
			file = list_entry(find, struct file, elem);
			if(file->fd == fd)
				break;
		}
		if(file == NULL){
			result = -1;
			return result;
		}
		
		result = file_length(file);
	

		return result;
}		

void munmap_clear(struct MMTE *m){
	lock_acquire(&F_lock);
	struct thread *curr = thread_current();
	struct SPTE *spte;
	

	uint8_t *temp_VMP = m->VMP;
	int file_size = m->length;

	while(file_size > 0){
		spte = find_SPT(curr, temp_VMP);

		ASSERT(spte != NULL);
		ASSERT(spte->status == 2 || spte->status == 3);

		if(spte->status == 3){
			lock_acquire(&sys_lock);
			mmap_writeback(spte);
			lock_release(&sys_lock);
		}
		delete_SPTE(spte);
		
		temp_VMP += PGSIZE;
		file_size -= PGSIZE;
	}
	lock_release(&F_lock);
}
