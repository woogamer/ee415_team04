#include "vm/FT.h"
#include "vm/SPT.h"
#include "vm/Swap.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "filesys/file.h"
#include "threads/vaddr.h"

void init_SPT(struct thread * t)
{
	list_init(&(t->SPT));
}


void push_SPT( struct FTE * fte)
{
 	//enum intr_level old_level;
        //old_level = intr_disable();
        
	struct thread * t = thread_current();
//	printf("sad1\n");
	struct SPTE * spte = malloc(sizeof(struct SPTE)); 
	
//	printf("sad2\n");
	spte->VMP = fte->VMP;
	spte->status=0;
	spte->FTE_elem = &fte->FTE_elem;
	spte->SWTE_elem = NULL;

	
	
//	printf("sad3\n");
	list_push_back(&t->SPT, &spte->SPTE_elem);
	//intr_set_level(old_level);

//	printf("sad4\n");
} 

struct SPTE * find_SPT(struct thread * t, uint8_t * VMP)
{
	
	//enum intr_level old_level;
        //old_level = intr_disable();
	
	struct list_elem *e;

	for(e= list_begin (&t->SPT); e != list_end (&t->SPT); e= list_next(e))
	{
		struct SPTE *spte = list_entry(e, struct SPTE, SPTE_elem);
		if(VMP==spte->VMP)
		return spte;
	}
	

	//intr_set_level(old_level);

	return NULL;

}

void delete_SPT(void)
{
	int count=0;
	struct thread * t= thread_current();
	struct list_elem *e;
	for(e= list_begin (&t->SPT); e != list_end (&t->SPT); )
	{
	struct SPTE *spte = list_entry(e, struct SPTE, SPTE_elem);
	e=list_remove(e);
	free(spte);
	count++;
	}

	ASSERT(0==list_size(&t->SPT));
	
}

void delete_SPTE(struct SPTE * spte){
	//if(is_interior(&spte->SPTE_elem))
		list_remove(&spte->SPTE_elem);
	free(spte);
}

void mmap_writeback(struct SPTE *spte){
	struct thread *curr = thread_current();
	spte->status = 2;

	if(pagedir_is_dirty (curr->pagedir, spte->VMP)){
		file_seek(spte->file, spte->offset);
		file_write(spte->file, spte->VMP, spte->endaddr - spte->VMP);
	}

}
