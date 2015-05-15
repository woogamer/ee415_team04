#include "vm/FT.h"
#include "vm/SPT.h"
#include "vm/Swap.h"
#include "threads/malloc.h"
#include <stdio.h>

void init_SPT(struct thread * t)
{
	list_init(&(t->SPT));
}


void push_SPT( struct FTE * fte)
{

 	enum intr_level old_level;
        old_level = intr_disable();
        
	struct thread * t = thread_current();

	struct SPTE * spte = malloc(sizeof(struct SPTE)); 
	spte->VMP = fte->VMP;
	spte->status=0;
	spte->FTE_elem = &fte->FTE_elem;
	spte->SWTE_elem = NULL;
	
	
	list_push_back(&t->SPT, &spte->SPTE_elem);
	intr_set_level(old_level);
} 

struct SPTE * find_SPT(uint8_t * VMP)
{
	struct thread * t = thread_current();
	
	enum intr_level old_level;
        old_level = intr_disable();
	
	struct list_elem *e;

	for(e= list_begin (&t->SPT); e != list_end (&t->SPT); e= list_next(e))
	{
		struct SPTE *spte = list_entry(e, struct SPTE, SPTE_elem);
		if(VMP==spte->VMP)
		return spte;
	}
	

	intr_set_level(old_level);

	return NULL;

}

