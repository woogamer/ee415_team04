#include "vm/FT.h"
#include "vm/SPT.h"
#include "vm/Swap.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "threads/vaddr.h"

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
	//0x5a5a5000
	ASSERT(pg_round_down(VMP)!=0x5a5a5000);
	//printf("VMP = %x\n ",VMP );
	struct thread * t = thread_current();
	uint8_t * PMP =palloc_get_page(flag);
	//printf("F_alloc  tid =%d F_size=%d\n", t->tid, list_size(&FT));	

	//printf("F_alloc  tid =%d \n", t->tid);	
	//printf("\nFT size = %d\n", list_size(&FT));

	if(PMP==NULL)
	{
		//printf("evict\n");
		PMP=evict();
		//printf("evict!!!\n");
	}
	struct FTE * fte = malloc (sizeof(struct FTE));
	fte-> t = thread_current();
	fte->VMP = VMP;
	fte->PMP = PMP;
	//printf("%p %p\n\n",VMP,PMP);
	//printf("success\n");
	//enum intr_level old_level;
        //old_level = intr_disable();
	
	//printf("KKKKKKKKKKKKKKKKKKKL1 \n");
	//printf("KKKKKKKKKKKKKKKKKKKL1 F_size=%d\n", list_size(&FT));
	list_push_back(&FT, &fte->FTE_elem);

	//printf("\nFFFFFFFFFFFT size = %d\n", list_size(&FT));

	//printf("KKKKKKKKKKKKKKKKKKKL2\n");
	ASSERT(373>=list_size(&FT));

	//printf("KKKKKKKKKKKKKKKKKKKL3\n");
	//intr_set_level(old_level);
	//update SPT
	push_SPT(fte);
	
	//printf("KKKKKKKKKKKKKKKKKKKL\n");
	//printf("KKKKKKKKKKKKKKKKKKKL F_size=%d\n", list_size(&FT));
	//printf("success\n");
	return PMP;
}

void delete_FT()
{
	struct thread * t= thread_current();
	struct list_elem *e;

	for(e= list_begin (&FT); e != list_end (&FT); )
	{
		struct FTE *fte = list_entry(e, struct FTE, FTE_elem);
		if(t==fte->t)
		{
			e=list_remove(e);
			free(fte);
		}
		else
		e=list_next(e);
	}
	
}


	

