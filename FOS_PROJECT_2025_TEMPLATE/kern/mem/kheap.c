#include "kheap.h"
#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#define MAX_PHYS_FRAMES 1048576  // supports up to 4GB / 4KB pages (1M frames)
static uint32 phys_to_virt[MAX_PHYS_FRAMES] ;   // Reverse mapping table
static uint32 phy_alloc_start[MAX_PHYS_FRAMES];


//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)

void kheap_init()
{
//==================================================================================
//DON'T CHANGE THESE LINES==========================================================
//==================================================================================
	initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
	set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
	kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
	kheapPageAllocBreak = kheapPageAllocStart;
	for (uint32 i = 0; i < MAX_PHYS_FRAMES; ++i){
		phys_to_virt[i] = 0;
		phy_alloc_start[i] = 0;
	}

}


bool is_page_free(uint32 va)
{
	uint32 *ptr = NULL;
	struct FrameInfo *f = get_frame_info(ptr_page_directory, va, &ptr);
	return (f==NULL);
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
    int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
    if (ret < 0)
        panic("get_page() in kern: failed to allocate page from the kernel");

    /* Record mapping (for O(1) reverse lookup): by default point to the page's VA.
       For a multi-page allocation we will overwrite these to the alloc_start in kmalloc(). */
    uint32 phy = kheap_physical_address((uint32)va);
    if (phy != 0)
    {
        uint32 frame = phy >> 12;
        if (frame < MAX_PHYS_FRAMES)
            phys_to_virt[frame] = ROUNDDOWN((uint32)va, PAGE_SIZE);
        phy_alloc_start[frame] = 0;
    }

    return 0;
}



void return_page(void* va)
{
	uint32 phy = kheap_physical_address((uint32)va);
	if (phy != 0)
	{
		uint32 frame = phy >> 12;
		if (frame < MAX_PHYS_FRAMES)
				phys_to_virt[frame] = 0;
	}
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
void* kmalloc(unsigned int size){

//Check if size <= 2K (Block Allocate)
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
		return alloc_block(size);

	//else if size > 2k (Page Allocate)
	else if(size > DYN_ALLOC_MAX_BLOCK_SIZE){


		uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);

		if (PAGE_SIZE == 0) {
						    panic("Invalid allocation size = 0");
						    return NULL;
						}
		uint32 num_pages = rounded_size / PAGE_SIZE;


		if (kheapPageAllocBreak == kheapPageAllocStart){
			uint32 alloc_start = kheapPageAllocBreak;
			while(kheapPageAllocBreak < (kheapPageAllocStart + size) ){
				get_page((void*)kheapPageAllocBreak);
				uint32 pa = kheap_physical_address(kheapPageAllocBreak);
					if (pa != 0) {
						uint32 frame = pa >> 12;
						if (frame < MAX_PHYS_FRAMES){
							phys_to_virt[frame] = kheapPageAllocBreak;
							phy_alloc_start[frame] = alloc_start;
						}
					}
					kheapPageAllocBreak += PAGE_SIZE;
			}
			return (void *)alloc_start;
		}

		uint32 first_free = 0;
		uint32 first_used = 0;
		uint32 best_start = 0;
		uint32 best_size = 0;
		bool found_exact = 0;
		bool f1 = 1;
		bool f2 = 1;

		for (uint32 i = kheapPageAllocStart;i < kheapPageAllocBreak; i += PAGE_SIZE){
			if (is_page_free(i) && f1) {
				first_free = i;
				f1 = 0;
				f2 = 1;
			}
			else if (!is_page_free(i) && f2 && !(f1)){
				first_used = i;
				f2 = 0;
				uint32 gap = first_used - first_free;
				if (gap == rounded_size){
					best_start = first_free;
					found_exact = 1;
					break;
				} else if (gap > best_size){
				best_start = first_free;
				best_size = gap;
					}
				f1 = 1;
				f2 = 1;
			}
		}
		if (!f1 && f2){
			uint32 gap = kheapPageAllocBreak - first_free;
			if (gap == rounded_size){
				best_start = first_free;
				found_exact = 1;
			} else if (gap > best_size){
				best_start = first_free;
				best_size = gap;
			}
		}
		if (found_exact || best_size >= rounded_size){
			uint32 alloc_start = best_start;
			uint32 alloc_end = alloc_start + rounded_size;
			for (uint32 va = alloc_start; va < alloc_end; va += PAGE_SIZE){
				get_page((void*)va);
				uint32 pa = kheap_physical_address(va);
				if (pa != 0){
					uint32 frame = pa >> 12;
					if(frame < MAX_PHYS_FRAMES){
						phys_to_virt[frame] = va;
						phy_alloc_start[frame] = alloc_start;
					}
				}
			}
			return (void*)alloc_start;
		}

		if (rounded_size <= (KERNEL_HEAP_MAX - kheapPageAllocBreak)){
			uint32 alloc_start = kheapPageAllocBreak;

			for(uint32 i = 0; i < num_pages; i++){
				if(kheapPageAllocBreak + PAGE_SIZE > KERNEL_HEAP_MAX){
					for(uint32 j = alloc_start; j < kheapPageAllocBreak; j += PAGE_SIZE){
						uint32 pa2 = kheap_physical_address(j);
						if(pa2){
							uint32 f2 = pa2 >> 12;
							if(f2 < MAX_PHYS_FRAMES){
								phys_to_virt[f2] = 0;
								phy_alloc_start[f2] = 0;
							}
						}unmap_frame(ptr_page_directory, j);
					}
					kheapPageAllocBreak = alloc_start;
					return NULL;
				}
				uint32 va = kheapPageAllocBreak;
				get_page((void*)va);

				uint32 pa = kheap_physical_address(va);
				if(pa != 0){
					uint32 frame = pa >> 12;
					if(frame < MAX_PHYS_FRAMES){
						phys_to_virt[frame] = va;
						phy_alloc_start[frame] = alloc_start;
					}
				}
				kheapPageAllocBreak += PAGE_SIZE;
			}
			return (void*)alloc_start;
			}
		}
	return NULL;
}
//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address){
    if (virtual_address ==NULL)
       return ;

//    b3rf variable to cheack el 2 7alat(page allocator,block allacator)
    uint32 va = (uint32)virtual_address;

    if (va>= KERNEL_HEAP_START && va< dynAllocEnd){
        free_block(virtual_address);
        return;
    }
//    cheack el page allocation tol ma address
    if (va>=kheapPageAllocStart && va < kheapPageAllocBreak ){
//    	dh hna 3shan admn enu yrg3ly bdayet el page msh middle page
        uint32 page_va = ROUNDDOWN(va, PAGE_SIZE);

        uint32 pa = kheap_physical_address(page_va);
        if (pa==0) return;
//         bgeb el frame noumber b shift el 12 bit  3shan ageb el physical address
        uint32 frame = pa >>12;
        if (frame>= MAX_PHYS_FRAMES) return;

//        el bdaya el f3lya lya
        uint32 real_start =phy_alloc_start[frame];

//hnaa lw tl3t 0 bnbos 3la reverse table lw hwa kman b 0 ybqa frame dh msh mtkhzn 3ndy
        if(real_start == 0){
           real_start = phys_to_virt[frame];
           if(real_start ==0) return;
        }


        uint32 cur = real_start ;
        while(cur < kheapPageAllocBreak){
              uint32 cur_pa = kheap_physical_address(cur);
              if(cur_pa ==0) break;
              uint32 cur_frame = cur_pa >>12;
              if (cur_frame >= MAX_PHYS_FRAMES) break;

              if (phy_alloc_start[cur_frame] != real_start) break;

              phys_to_virt[cur_frame]=0;
              phy_alloc_start[cur_frame]=0;
              unmap_frame(ptr_page_directory, cur);

              cur += PAGE_SIZE;
         }

         while (kheapPageAllocBreak > kheapPageAllocStart){
              uint32 last_page = kheapPageAllocBreak - PAGE_SIZE ;
              uint32 last_pa = kheap_physical_address(last_page);

              if (last_pa ==0){
                  kheapPageAllocBreak -= PAGE_SIZE;
                  continue;
              }
              uint32 last_frame = last_pa >>12;
              if (last_frame>= MAX_PHYS_FRAMES) break;
              if (phy_alloc_start[last_frame] ==0){

                  phys_to_virt[last_frame] =0;
                  unmap_frame(ptr_page_directory, last_page);
                  kheapPageAllocBreak -= PAGE_SIZE;
                  continue;
              }
                  break;
          }
          return;
     }
     return;
}


//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
//Your code is here
//Comment the following line
//panic("kheap_virtual_address() is not implemented yet...!!");

//	bnhndl bs lw el address invalid
	if (physical_address == 0) return 0;

//	hna b shift el 12 bto3 el offset 3shan a3rf ageb el frame
	uint32 frame = physical_address >> 12;

//	b cheack lw el frame akbr mn max frames el mt3rfa define 3ndy b return 0
	if (frame >= MAX_PHYS_FRAMES) return 0;

//	3nwan el virtual le bdayet el frame dh
	uint32 vpage = phys_to_virt[frame];

	if (vpage == 0) return 0;

//	b calculate el offset el page ely 3ndnaa
	uint32 offset = physical_address & (PAGE_SIZE - 1);
	return vpage + offset;

/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
//Your code is here
//Comment the following line
//panic("kheap_physical_address() is not implemented yet...!!");

//	rounddown 3shan a3rf begin of page
uint32 page_va = ROUNDDOWN(virtual_address, PAGE_SIZE);

//3rft pointer ely ht7km fe el directory able
uint32 *ptr_page_table = NULL;

//estkhdmt ell functionn get frame bb3tlha virtual address w el refrence bta3 el pointer ely hyt7rk beh 3shan tgbly frame ely marbout be el addresa
struct FrameInfo *frame = get_frame_info(ptr_page_directory, page_va, &ptr_page_table);
if (frame == NULL) return 0;
uint32 frame_pa = to_physical_address(frame);
uint32 offset = virtual_address & (PAGE_SIZE - 1);
return frame_pa + offset;

/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
//Your code is here
//Comment the following line
//panic("krealloc() is not implemented yet...!!");
	if (virtual_address==NULL)
		return kmalloc(new_size);
	if(new_size==0){
		kfree(virtual_address);
		return NULL ;
	}
	uint32 va_int = (uint32)virtual_address;

	//case 1 :block allocator
	if(va_int >= KERNEL_HEAP_START && va_int < dynAllocEnd){
		uint32 old_size =get_block_size(virtual_address);
		void* new_ptr =kmalloc(new_size);
		if(!new_ptr)
			return NULL ;
		memcpy(new_ptr, virtual_address, MIN(old_size, new_size));
		kfree(virtual_address);
		return new_ptr;

	}
	//case 2 :page allocator
	if(va_int >= kheapPageAllocStart && va_int < kheapPageAllocBreak){
	    uint32 page = ROUNDDOWN(va_int, PAGE_SIZE);
		uint32 pa = kheap_physical_address(page);
		if(pa == 0)
			return NULL;
		uint32 frame = pa>>12;
		uint32 alloc_start = phy_alloc_start[frame];
		if(alloc_start == 0)
			alloc_start = phys_to_virt[frame];
		uint32 cur = alloc_start;
		while(1){
			uint32 pa2 = kheap_physical_address(cur);
			if(!pa2)
				break;
			uint32 fr2 = pa2 >> 12 ;
			if(phy_alloc_start[fr2] != alloc_start)
				break;
			cur += PAGE_SIZE;
		}
		uint32 old_size = cur - alloc_start;
		uint32 old_r = ROUNDUP(old_size, PAGE_SIZE);
		uint32 new_r = ROUNDUP(new_size, PAGE_SIZE);
		//case 1 : kant same size
		if(old_r == new_r)
			return virtual_address;

		//case 2 :kant akbar
		if(new_r >old_r){
			uint32 startn = alloc_start + old_r;
			uint32 endn = alloc_start + new_r;

			bool grow = 1;
			for(uint32 v = startn; v< endn; v+= PAGE_SIZE)
				if(!is_page_free(v))
					grow = 0;
			if(grow){
				for(uint32 v = startn; v< endn; v+= PAGE_SIZE){
					get_page((void*)v);
					uint32 pa3 = kheap_physical_address(v);
					uint32 fr3 = pa3 >> 12;
					phy_alloc_start[fr3] = alloc_start;
				}
				return virtual_address;
			}
		}
		//case 3 : nharak el block zat nafso
		void* new_ptr = kmalloc(new_size);
		if(!new_ptr)
			return NULL;

		memcpy(new_ptr, (void*)alloc_start, MIN(old_size, new_size));
		kfree((void*)alloc_start);
		return new_ptr;
	}
return NULL;
}
