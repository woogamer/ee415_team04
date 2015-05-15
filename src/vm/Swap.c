#include "vm/FT.h"
#include "vm/SPT.h"
#include "vm/Swap.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
void init_swap()
{
	list_init(&SWT);
	swap_map= bitmap_create(disk_size(disk_get(1,1)));

}

uint8_t * evict()
{
	//victim selection
	struct list_elem * elem = list_pop_front(&FT);
	struct FTE * victim = list_entry(elem, struct FTE, FTE_elem);
	//printf("victim PMP= %x VMP= %x \n, ", victim-> PMP, victim-> VMP);
	size_t idx =disk_in(victim);
	//printf("idx = %d\n", idx);
	uint8_t *PMP= victim->PMP;
	
	enum intr_level old_level;
	old_level = intr_disable();
	//SWT upadate
	struct SWTE * swte = malloc(sizeof(struct SWTE));
	swte->t = victim->t;
	swte-> VMP = victim->VMP;
	swte-> disk_idx= idx;
	//printf("SWTE VMP= %x\n",swte->VMP );

	list_push_back(&SWT, &swte->SWTE_elem);
	//SPT update
	struct SPTE * spte = find_SPT(victim->VMP);
	ASSERT(spte->status==0)	
	spte->status=1;
	spte->SWTE_elem = &swte->SWTE_elem;
	spte->FTE_elem = NULL;

	//PT update
	list_remove(&victim->FTE_elem);
	
	intr_set_level(old_level);

	//pagedir update
	pagedir_clear_page(victim->t->pagedir, victim->VMP);
	//physical memory update
	palloc_free_page(victim->PMP);
	
	free(victim);
	return PMP;
}


size_t disk_in(struct FTE * victim)
{
	size_t idx = bitmap_scan_and_flip(swap_map, 0, PGSIZE/512, false);	
	
	int i;
	for (i=0; i<8; i++ )
	disk_write(disk_get(1,1), idx+i, victim->PMP+i*512);
	
	return idx;
}

void disk_out(struct thread * t, uint8_t * VMP, uint8_t *PMP)
{
	struct SWTE * swte = disk_find(t,VMP);
	
	ASSERT(swte);
	
	int i;
	for (i=0; i<8; i++ )
	disk_read(disk_get(1,1),swte->disk_idx+i , PMP+i*512);

	bitmap_set_multiple(swap_map, swte->disk_idx, 8, false);
	
	enum intr_level old_level;
        old_level = intr_disable();
	list_remove(&swte->SWTE_elem);
	intr_set_level(old_level);

	free(swte);

}




struct SWTE * disk_find(struct thread *t, uint8_t *VMP)
{

	enum intr_level old_level;
        old_level = intr_disable();

        struct list_elem *e;
	struct SWTE * swte=NULL;
        for(e= list_begin (&SWT); e != list_end (&SWT); e= list_next(e))
        {
                swte = list_entry(e, struct SWTE, SWTE_elem);
                if(swte->t==t && swte->VMP == VMP)
                return swte;
                
        }



        intr_set_level(old_level);
	return swte;
}

