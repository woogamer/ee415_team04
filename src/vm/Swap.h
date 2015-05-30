#ifndef VM_SWAP_H
#define VM_SWAP_H

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
#include "vm/SPT.h"

struct list SWT;
struct bitmap *swap_map;

//swap table Entry
struct SWTE
{
	struct thread * t;
	uint8_t * VMP;
	disk_sector_t disk_idx;
	struct list_elem SWTE_elem;
};

void init_swap(void);

uint8_t * evict(void);
size_t disk_in(struct FTE * victim);
void disk_out(struct thread * t, uint8_t * VMP, uint8_t *PMP);
struct SWTE * disk_find(struct thread *t, uint8_t *VMP);
void delete_SWT(void);
#endif
