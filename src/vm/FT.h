#ifndef VM_FT_H
#define VM_FT_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

struct list FT;
struct lock F_lock;

//Frame table entry
struct FTE
{
	struct thread * t;		//thread id
	uint8_t * VMP;		//virtual memory page
	uint8_t * PMP;		//physical memory page
	struct list_elem FTE_elem; //Frame Table Elem
	
};

void init_vm(void);
void init_FT(void);
uint8_t * F_alloc(uint8_t * VMP, enum palloc_flags flag);
void delete_FT(void);
#endif
