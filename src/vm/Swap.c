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

	//printf("evict start \n");
	//victim selection
	struct list_elem * elem = list_pop_front(&FT);
	
	//printf("victim selection \n");
	struct FTE * victim = list_entry(elem, struct FTE, FTE_elem);

	//printf("diskin start \n");
	size_t idx =disk_in(victim);
	//printf("]]]]]]%d\n",list_size(&FT));
	//printf(" idx = %d\n", idx);
	uint8_t *PMP= victim->PMP;
	//printf("victim next = %p\n", elem->next);
	//printf("victim prev = %p\n", elem->prev);
  	
	//enum intr_level old_level;
	//old_level = intr_disable();
	//SWT upadate
	struct SWTE * swte = malloc(sizeof(struct SWTE));
	swte->t = victim->t;
	swte-> VMP = victim->VMP;
	swte-> disk_idx= idx;
	//printf("PID= %d, SWTE VMP= %x\n",swte->t->tid, swte->VMP );

	list_push_back(&SWT, &swte->SWTE_elem);
/*
	struct list_elem *e;
	for(e=list_begin (&SWT); e != list_end (&SWT);
           e = list_next (e))
        {
          struct SWTE *temp = list_entry(e, struct SWTE, SWTE_elem);
          printf(" [elem = %p] --", temp->VMP);
        }

*/
//printf("list_size = %d\n", list_size(&FT));
/*
struct list * list = &FT;
  size_t cnt = 0;

  for (e = list_begin (list); e != list_end (list); e = list_next (e))
  {
struct FTE * temp = list_entry(e, struct FTE, FTE_elem);
	printf("PMP = %p , victim_PMP = %p\n", temp->PMP, &victim->PMP);
	if(temp->PMP==victim->PMP)
	break;

	cnt++;

printf("position = %d\n", cnt);
	}
*/



  	//SPT update
  
	struct SPTE * spte = find_SPT(victim->t, victim->VMP);
	ASSERT(spte!=NULL);
	ASSERT(spte->status==0)	
	spte->status=1;
	spte->SWTE_elem = &swte->SWTE_elem;
	spte->FTE_elem = NULL;
	//printf("FT SIZE Before remove = %d\n", list_size(&FT));
	//FT update
	//list_remove(&victim->FTE_elem);
	
	//intr_set_level(old_level);

	//pagedir update
	pagedir_clear_page(victim->t->pagedir, victim->VMP);
	//physical memory update
	//palloc_free_page(victim->PMP);
	//palloc_get_page(flag);

	free(victim);
	return PMP;
}


size_t disk_in(struct FTE * victim)
{
	//printf("START TO DISK IN\n");
	size_t idx = bitmap_scan_and_flip(swap_map, 0, PGSIZE/512, false);	
	
	int i;
	for (i=0; i<8; i++ )
	disk_write(disk_get(1,1), idx+i, victim->PMP+i*512);
	
	//printf("END TO DISK IN\n");
	return idx;
}

void disk_out(struct thread * t, uint8_t * VMP, uint8_t *PMP)
{

	//printf("START TO DISK OUT\n");
	struct SWTE * swte = disk_find(t,VMP);
	
	ASSERT(swte);
	
	int i;
	for (i=0; i<8; i++ )
	disk_read(disk_get(1,1),swte->disk_idx+i , PMP+i*512);

	bitmap_set_multiple(swap_map, swte->disk_idx, 8, false);
	
	//enum intr_level old_level;
        //old_level = intr_disable();
	list_remove(&swte->SWTE_elem);
	//intr_set_level(old_level);

	free(swte);

	//printf("END TO DISK OUT\n");
}




struct SWTE * disk_find(struct thread *t, uint8_t *VMP)
{

	//enum intr_level old_level;
        //old_level = intr_disable();

        struct list_elem *e;
	struct SWTE * swte=NULL;
        for(e= list_begin (&SWT); e != list_end (&SWT); e= list_next(e))
        {
                swte = list_entry(e, struct SWTE, SWTE_elem);
                if(swte->t==t && swte->VMP == VMP)
                return swte;
                
        }



        //intr_set_level(old_level);
	return swte;
}

void delete_SWT()
{
//int count=0;
	struct thread * t= thread_current();
	struct list_elem *e;
//printf("SWT start\n");
	for(e= list_begin (&SWT); e != list_end (&SWT);)
	{

//printf("SWT count=%d\n", count);
		struct SWTE *swte = list_entry(e, struct SWTE, SWTE_elem);
		if(t==swte->t)
		{
			e=list_remove(e);
			free(swte);
		}
		else
		e=list_next(e);
	}
	
//printf("SWT end\n");
}
