#ifndef VM_SPTE_H
#define VM_SPTE_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/disk.h"
#include <bitmap.h>
#include "threads/interrupt.h"
#include "vm/FT.h"
#include "vm/Swap.h"

#define SWAPPED 1
#define PHYSICAl 0


struct SPTE
{
        uint8_t *VMP;
	int status; //0=normal 1=swapped 2=mmap not load 3= mmap load
	struct list_elem *FTE_elem;
        struct list_elem *SWTE_elem;
	struct list_elem SPTE_elem;

	//mmap
	struct file *file;
	uint8_t * endaddr;
	int offset;
};

struct MMTE
{
	int id;				//id
	struct file *file;		//file
	int length;			//file length
	uint8_t * VMP;			//start address
	struct list_elem MMTE_elem;	//elem
};

void init_SPT(struct thread * t);
void push_SPT(struct FTE * fte);
struct SPTE * find_SPT(struct thread *t, uint8_t *VMP);
void delete_SPTE(struct SPTE * spte);
void delete_SPT(void);
void mmap_writeback(struct SPTE *spte);

#endif
