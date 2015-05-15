#include "vm/FT.h"
#include "vm/SPT.h"
#include "vm/Swap.h"
#include "threads/malloc.h"
#include <stdio.h>

void init_vm()
{
	init_FT();
	init_swap();
}

void init_FT()
{
	list_init(&FT);
	lock_init(&F_lock);
}
uint8_t * F_alloc(uint8_t * VMP, enum palloc_flags flag)
{
	
	printf("VMP = %x\n ",VMP );

	struct thread * t = thread_current();
	uint8_t * PMP =palloc_get_page(flag);
	if(PMP==NULL)
	{
		PMP=evict();
		printf("evict\n");
	}
	struct FTE * fte = malloc (sizeof(struct FTE));
	fte-> t = thread_current();
	fte->VMP = VMP;
	fte->PMP = PMP;

	//printf("success\n");
	enum intr_level old_level;
        old_level = intr_disable();
	list_push_back(&FT, &fte->FTE_elem);
	intr_set_level(old_level);
	//update SPT
	push_SPT(fte);
	
	//printf("success\n");
	return PMP;
}


	

