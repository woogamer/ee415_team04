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
	int status;
	struct list_elem *FTE_elem;
        struct list_elem *SWTE_elem;
	struct list_elem SPTE_elem;
};

void init_SPT(struct thread * t);
void push_SPT(struct FTE * fte);
struct SPTE * find_SPT(struct thread *t, uint8_t *VMP);

#endif
